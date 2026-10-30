#include "pch.h"
#include "StackSymbolizer.h"
#include "AppSettings.h"
#include <TraceSession.h>
#include <DbgHelp.h>
#include <Psapi.h>
#include <future>
#include <thread>

#pragma comment(lib, "dbghelp")

namespace {
	// fake DbgHelp context for kernel modules (not a real process handle)
	const HANDLE KernelContext = reinterpret_cast<HANDLE>(0x4B524E4CULL);

	bool IsKernelAddress(ULONG64 address) {
		return address >= 0xFFFF800000000000ULL;
	}

	// TimeDateStamp and SizeOfImage from the file's PE header ({0, 0} if it isn't one).
	// The size is needed for loading: with deferred loads, a module loaded without one covers no addresses.
	// SizeOfImage is at the same offset in 32 and 64 bit optional headers.
	std::pair<DWORD, DWORD> GetImageId(std::wstring const& path) {
		wil::unique_hfile hFile(::CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr));
		if (!hFile)
			return {};
		BYTE buffer[4096];
		DWORD read = 0;
		if (!::ReadFile(hFile.get(), buffer, sizeof(buffer), &read, nullptr))
			return {};
		auto dos = reinterpret_cast<IMAGE_DOS_HEADER const*>(buffer);
		if (read < sizeof(*dos) || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > read)
			return {};
		auto nt = reinterpret_cast<IMAGE_NT_HEADERS64 const*>(buffer + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE)
			return {};
		return { nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage };
	}
}

StackSymbolizer& StackSymbolizer::Get() {
	// never destroyed: its thread may be in a download when the process exits
	static auto symbolizer = new StackSymbolizer;
	return *symbolizer;
}

StackSymbolizer::StackSymbolizer() : m_Thread([this] { Run(); }) {
}

void StackSymbolizer::Run() {
	m_ThreadId = ::GetCurrentThreadId();
	while (!m_Quit) {
		Task task;
		{
			std::unique_lock locker(m_Lock);
			m_Ready.wait(locker, [&] { return !m_Tasks.empty(); });
			task = std::move(m_Tasks.front());
			m_Tasks.pop_front();
			m_RunningOwner = task.Owner;
			m_CancelRunning = false;
		}
		task.Work();
		SetActivity(L"");
		std::lock_guard locker(m_Lock);
		m_RunningOwner = nullptr;
	}
}

void StackSymbolizer::Post(void const* owner, std::function<void()> work) {
	{
		std::lock_guard locker(m_Lock);
		m_Tasks.push_back({ owner, std::move(work) });
	}
	m_Ready.notify_one();
}

void StackSymbolizer::Cancel(void const* owner) {
	std::lock_guard locker(m_Lock);
	std::erase_if(m_Tasks, [&](auto const& task) { return task.Owner == owner; });
	if (m_RunningOwner == owner)
		m_CancelRunning = true;
}

StackSymbolizer::Frame StackSymbolizer::MakeFrame(TraceSession const* session, DWORD pid, ULONG64 address, LONGLONG time) {
	Frame frame{ address, pid };
	// drivers are images of process 0
	if (session)
		frame.Image = session->FindImage(IsKernelAddress(address) ? 0 : pid, address, time);
	return frame;
}

void StackSymbolizer::Request(void const* owner, TraceSession const* session, std::vector<Frame> frames, SymbolCallback callback) {
	Post(owner, [=, this, frames = std::move(frames), callback = std::move(callback)] {
		for (size_t i = 0; i < frames.size() && !m_CancelRunning; i++)
			callback(i, Resolve(session, frames[i]));
	});
}

std::vector<std::wstring> StackSymbolizer::GetSymbols(TraceSession const* session, std::vector<Frame> frames, std::atomic<bool> const* cancel) {
	ATLASSERT(::GetCurrentThreadId() != m_ThreadId);
	// (an export asks for each event's frames until it sees the cancel itself)
	if (cancel && *cancel)
		return {};
	// shared with the lookup: a canceled caller doesn't wait for it, and may be gone when it runs
	struct Job {
		std::vector<Frame> Frames;
		std::promise<std::vector<std::wstring>> Result;
		std::atomic<bool> Canceled{ false };
	};
	auto job = std::make_shared<Job>();
	job->Frames = std::move(frames);
	auto result = job->Result.get_future();
	Post(nullptr, [=, this] {
		std::vector<std::wstring> symbols;
		symbols.reserve(job->Frames.size());
		m_JobCancel = &job->Canceled;
		for (auto& frame : job->Frames) {
			if (job->Canceled)
				break;
			symbols.push_back(Resolve(session, frame));
		}
		m_JobCancel = nullptr;
		job->Result.set_value(std::move(symbols));
	});
	if (!cancel)
		return result.get();
	while (result.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout) {
		if (*cancel) {
			job->Canceled = true;
			return {};
		}
	}
	return result.get();
}

bool StackSymbolizer::Canceling() const noexcept {
	auto job = m_JobCancel.load();
	return m_CancelRunning || (job && *job);
}

void StackSymbolizer::Reset() {
	Post(nullptr, [this] { DoReset(); });
}

void StackSymbolizer::Shutdown() {
	{
		std::lock_guard locker(m_Lock);
		m_Tasks.clear();
		m_CancelRunning = true;
	}
	Post(nullptr, [this] {
		DoReset();
		m_Quit = true;
	});
	// a download can take longer: then the process ends with the thread still in it
	if (::WaitForSingleObject(m_Thread.native_handle(), 3000) == WAIT_OBJECT_0)
		m_Thread.join();
	else
		m_Thread.detach();
}

void StackSymbolizer::DoReset() {
	m_Cache.clear();
	m_ImageCache.clear();
	for (auto& [pid, hProcess] : m_Processes) {
		::SymCleanup(hProcess);
		::CloseHandle(hProcess);
	}
	m_Processes.clear();
	m_FailedProcesses.clear();
	for (auto& [key, context] : m_TraceContexts)
		::SymCleanup(context.Handle);
	m_TraceContexts.clear();
	if (m_KernelInit)
		::SymCleanup(KernelContext);
	m_KernelInit = false;
}

bool StackSymbolizer::InitContext(HANDLE hContext, bool invadeProcess) {
	static bool optionsSet = false;
	if (!optionsSet) {
		::SymSetOptions(::SymGetOptions() | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS);
		optionsSet = true;
	}
	// not null even when empty: DbgHelp would add _NT_SYMBOL_PATH, which the settings may exclude
	auto path = AppSettings::Get().BuildSymbolSearchPath();
	if (!::SymInitializeW(hContext, path.c_str(), invadeProcess))
		return false;
	::SymRegisterCallbackW64(hContext, OnSymbolEvent, reinterpret_cast<ULONG64>(this));
	return true;
}

BOOL CALLBACK StackSymbolizer::OnSymbolEvent(HANDLE, ULONG action, ULONG64 data, ULONG64 context) {
	auto self = reinterpret_cast<StackSymbolizer*>(context);
	switch (action) {
		case CBA_DEFERRED_SYMBOL_LOAD_START: {
			std::wstring_view file = reinterpret_cast<PIMAGEHLP_DEFERRED_SYMBOL_LOADW64>(data)->FileName;
			self->SetActivity(std::format(L"Loading symbols for {}", file.substr(file.find_last_of(L'\\') + 1)));
			break;
		}
		case CBA_DEFERRED_SYMBOL_LOAD_COMPLETE:
		case CBA_DEFERRED_SYMBOL_LOAD_FAILURE:
			self->SetActivity(L"");
			break;
		case CBA_DEFERRED_SYMBOL_LOAD_CANCEL:
			// asked now and then while symbols load (downloads included): TRUE stops it
			return self->Canceling();
	}
	// informational only (TRUE would mean handled: for a failure, retry with another file)
	return FALSE;
}

void StackSymbolizer::SetActivity(std::wstring text) {
	std::lock_guard locker(m_ActivityLock);
	m_Activity = std::move(text);
}

std::wstring StackSymbolizer::GetActivity() const {
	std::lock_guard locker(m_ActivityLock);
	return m_Activity;
}

std::wstring StackSymbolizer::DriverPathToDosPath(std::wstring path) {
	static const std::wstring systemRoot = L"\\SystemRoot\\";
	if (path.starts_with(systemRoot)) {
		WCHAR windir[MAX_PATH];
		::GetWindowsDirectory(windir, _countof(windir));
		return std::wstring(windir) + L"\\" + path.substr(systemRoot.size());
	}
	if (path.starts_with(L"\\??\\"))
		return path.substr(4);
	return path;
}

HANDLE StackSymbolizer::GetKernelContext() {
	if (m_KernelInit)
		return KernelContext;

	if (!InitContext(KernelContext, false))
		return nullptr;
	m_KernelInit = true;

	DWORD needed = 0;
	::EnumDeviceDrivers(nullptr, 0, &needed);
	std::vector<LPVOID> drivers(needed / sizeof(LPVOID) + 16);
	if (!::EnumDeviceDrivers(drivers.data(), (DWORD)(drivers.size() * sizeof(LPVOID)), &needed))
		return KernelContext;

	drivers.resize(needed / sizeof(LPVOID));
	for (auto base : drivers) {
		WCHAR path[MAX_PATH];
		if (!::GetDeviceDriverFileName(base, path, _countof(path)))
			continue;
		auto dosPath = DriverPathToDosPath(path);
		::SymLoadModuleExW(KernelContext, nullptr, dosPath.c_str(), nullptr, (DWORD64)base, GetImageId(dosPath).second, nullptr, 0);
	}
	return KernelContext;
}

HANDLE StackSymbolizer::GetProcessContext(DWORD pid) {
	if (auto it = m_Processes.find(pid); it != m_Processes.end())
		return it->second;
	if (m_FailedProcesses.contains(pid))
		return nullptr;

	auto hProcess = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (hProcess && InitContext(hProcess, true)) {
		m_Processes.insert({ pid, hProcess });
		return hProcess;
	}
	if (hProcess)
		::CloseHandle(hProcess);
	m_FailedProcesses.insert(pid);
	return nullptr;
}

std::wstring StackSymbolizer::Lookup(HANDLE hContext, ULONG64 address) {
	IMAGEHLP_MODULEW64 module{ sizeof(module) };
	if (!::SymGetModuleInfoW64(hContext, address, &module))
		return L"";
	BYTE buffer[sizeof(SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(WCHAR)];
	auto symbol = reinterpret_cast<PSYMBOL_INFOW>(buffer);
	symbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
	symbol->MaxNameLen = MAX_SYM_NAME;
	DWORD64 displacement = 0;
	if (::SymFromAddrW(hContext, address, &displacement, symbol))
		return std::format(L"{}!{}+0x{:X}", module.ModuleName, symbol->Name, displacement);
	return std::format(L"{}+0x{:X}", module.ModuleName, address - module.BaseOfImage);
}

std::wstring StackSymbolizer::Resolve(TraceSession const* session, Frame const& frame) {
	if (auto& image = frame.Image) {
		// the same image's frames have the same symbols in every process (ntdll's in all of them)
		ImageCacheKey key(image->FileName, image->TimeDateStamp, image->Size, frame.Address - image->Base);
		if (auto it = m_ImageCache.find(key); it != m_ImageCache.end())
			return it->second;
		auto text = DoResolve(session, frame);
		if (Canceling())
			ForgetImage(session, frame);	// loaded again (and its symbols looked for) next time
		else
			m_ImageCache[key] = text;
		return text;
	}
	CacheKey key(session, IsKernelAddress(frame.Address) ? 0 : frame.Pid, frame.Address);
	if (auto it = m_Cache.find(key); it != m_Cache.end())
		return it->second;
	auto text = DoResolve(session, frame);
	if (!Canceling())
		m_Cache[key] = text;
	return text;
}

std::wstring StackSymbolizer::DoResolve(TraceSession const* session, Frame const& frame) {
	auto address = frame.Address;
	auto kernel = IsKernelAddress(address);
	auto pid = kernel ? 0 : frame.Pid;
	auto& image = frame.Image;
	if (image) {
		if (auto text = LookupTraceImage(session, pid, *image, address))
			return *text;
	}
	if (auto hContext = kernel ? GetKernelContext() : GetProcessContext(pid)) {
		if (auto text = Lookup(hContext, address); !text.empty())
			return text;
	}
	if (image) {
		// no file for the image: at least its name, as DbgHelp names modules (no extension)
		auto name = image->FileName.substr(image->FileName.find_last_of(L'\\') + 1);
		return std::format(L"{}+0x{:X}", name.substr(0, name.find_last_of(L'.')), address - image->Base);
	}
	return L"";
}

std::optional<std::wstring> StackSymbolizer::LookupTraceImage(TraceSession const* session, DWORD pid, ImageInfo const& image, ULONG64 address) {
	auto it = m_TraceContexts.find({ session, pid });
	if (it == m_TraceContexts.end()) {
		TraceContext context{ reinterpret_cast<HANDLE>(m_NextTraceHandle) };
		if (!InitContext(context.Handle, false))
			return {};
		m_NextTraceHandle += 4;
		it = m_TraceContexts.insert({ { session, pid }, std::move(context) }).first;
	}
	auto& context = it->second;
	// the same base can hold another image later (or in a reused process ID)
	auto key = std::format(L"{}|{:X}|{:X}", image.FileName, image.TimeDateStamp, image.Size);
	if (context.Failed.contains(key))
		return {};
	if (auto loaded = context.Loaded.find(image.Base); loaded == context.Loaded.end() || loaded->second != key) {
		if (loaded != context.Loaded.end()) {
			::SymUnloadModule64(context.Handle, image.Base);
			context.Loaded.erase(loaded);
		}
		auto file = FindImageFile(context.Handle, image);
		if (file.empty() || !::SymLoadModuleExW(context.Handle, nullptr, file.c_str(), nullptr, image.Base, image.Size, nullptr, 0)) {
			if (!Canceling())		// (not found because the search was stopped)
				context.Failed.insert(key);
			return {};
		}
		context.Loaded[image.Base] = key;
	}
	auto text = Lookup(context.Handle, address);
	if (text.empty())
		return {};
	return text;
}

// the image's file as it was when traced: the one on disk if it's the same build, else from the symbol
// server (which keeps Microsoft's binaries by timestamp and size)
std::wstring StackSymbolizer::FindImageFile(HANDLE hContext, ImageInfo const& image) {
	std::wstring path;
	if (image.FileName.starts_with(L"\\Device\\"))
		path = TraceSession::GetDosNameFromNtName(image.FileName.c_str());
	else
		path = DriverPathToDosPath(image.FileName);
	if (!path.empty()) {
		auto [timeStamp, size] = GetImageId(path);
		if (size == image.Size && (image.TimeDateStamp == 0 || timeStamp == image.TimeDateStamp))
			return path;
	}
	if (image.TimeDateStamp) {
		auto name = image.FileName.substr(image.FileName.find_last_of(L'\\') + 1);
		WCHAR found[MAX_PATH];
		SetActivity(std::format(L"Finding {}", name));
		auto ok = ::SymFindFileInPathW(hContext, nullptr, name.c_str(), reinterpret_cast<PVOID>((ULONG_PTR)image.TimeDateStamp), image.Size, 0,
			SSRVOPT_DWORD, found, nullptr, nullptr);
		SetActivity(L"");
		if (ok)
			return found;
	}
	return L"";
}

void StackSymbolizer::ForgetImage(TraceSession const* session, Frame const& frame) {
	auto pid = IsKernelAddress(frame.Address) ? 0 : frame.Pid;
	if (auto it = m_TraceContexts.find({ session, pid }); it != m_TraceContexts.end()) {
		auto& context = it->second;
		if (context.Loaded.erase(frame.Image->Base))
			::SymUnloadModule64(context.Handle, frame.Image->Base);
	}
}

void StackSymbolizer::Forget(TraceSession const* session) {
	// after the session's queued lookups, which use it only as a key
	Post(nullptr, [=, this] {
		std::erase_if(m_Cache, [&](auto const& entry) { return std::get<0>(entry.first) == session; });
		for (auto it = m_TraceContexts.begin(); it != m_TraceContexts.end(); ) {
			if (it->first.first == session) {
				::SymCleanup(it->second.Handle);
				it = m_TraceContexts.erase(it);
			}
			else
				++it;
		}
	});
}

#include "pch.h"
#include "StackSymbolizer.h"
#include "AppSettings.h"
#include <TraceSession.h>
#include <DbgHelp.h>
#include <Psapi.h>

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
	static StackSymbolizer symbolizer;
	return symbolizer;
}

StackSymbolizer::~StackSymbolizer() {
	Reset();
}

void StackSymbolizer::Reset() {
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
	return ::SymInitializeW(hContext, path.c_str(), invadeProcess);
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

std::wstring StackSymbolizer::GetSymbol(TraceSession const* session, DWORD pid, ULONG64 address, LONGLONG time) {
	auto kernel = IsKernelAddress(address);
	std::optional<ImageInfo> image;
	if (session) {
		// drivers are images of process 0
		image = session->FindImage(kernel ? 0 : pid, address, time);
		if (image) {
			if (auto text = LookupTraceImage(session, kernel ? 0 : pid, *image, address))
				return *text;
		}
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
		if (::SymFindFileInPathW(hContext, nullptr, name.c_str(), reinterpret_cast<PVOID>((ULONG_PTR)image.TimeDateStamp), image.Size, 0,
			SSRVOPT_DWORD, found, nullptr, nullptr))
			return found;
	}
	return L"";
}

void StackSymbolizer::Forget(TraceSession const* session) {
	for (auto it = m_TraceContexts.begin(); it != m_TraceContexts.end(); ) {
		if (it->first.first == session) {
			::SymCleanup(it->second.Handle);
			it = m_TraceContexts.erase(it);
		}
		else
			++it;
	}
}

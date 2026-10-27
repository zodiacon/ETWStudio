#include "pch.h"
#include <initguid.h>
#include "TraceSession.h"
#include "EventData.h"
#include <assert.h>
#include <algorithm>

#pragma comment(lib, "tdh")

TraceSession::TraceSession(std::wstring name) : m_SessionName(std::move(name)) {
}

bool TraceSession::OpenFile(PCWSTR path, EventCallback cb) {
	m_StoreEvents = false;
	m_LogFileName = path;
	m_Callback = cb;
	return Start(cb);
}

bool TraceSession::OpenFile(PCWSTR path, EventCallbackNoOwn cb) {
	m_StoreEvents = true;
	m_LogFileName = path;
	m_CallbackNoOwn = cb;
	return Start(cb);
}

TraceSession::~TraceSession() {
	Stop(true);
}

std::wstring const& TraceSession::SessionName() const noexcept {
	return m_SessionName;
}

std::wstring const& TraceSession::LogFileName() const noexcept {
	return m_LogFileName;
}

bool TraceSession::IsDone() const noexcept {
	return m_Done;
}

FilterManager& TraceSession::GetFilterManager() noexcept {
	return m_FilterMgr;
}

void TraceSession::SetFilterManager(FilterManager mgr) {
	std::lock_guard locker(m_FilterLock);
	m_FilterMgr = std::move(mgr);
}

void TraceSession::ClearEvents(std::function<void()> const& onClear) {
	std::lock_guard locker(m_EventsLock);
	if (onClear)
		onClear();
	m_RecentByThread.clear();
	m_PendingUserStacks.clear();
	m_PendingUserStackOrder.clear();
	m_Events.clear();
}

std::vector<EventData*> TraceSession::SnapshotEvents(std::function<void()> const& onSnapshot) {
	std::lock_guard locker(m_EventsLock);
	if (onSnapshot)
		onSnapshot();
	std::vector<EventData*> events;
	events.reserve(m_Events.size());
	for (auto& evt : m_Events)
		events.push_back(evt.get());
	return events;
}

bool TraceSession::SetSessionName(std::wstring name) noexcept {
	if (m_hTrace)
		return false;

	m_SessionName = std::move(name);
	return true;
}

GUID const* TraceSession::GetProviderGuid(std::wstring const& name) noexcept {
	if (!EnumProviders())
		return nullptr;

	if (name[0] == L'{') {
		thread_local static GUID guid;
		return S_OK == ::CLSIDFromString(name.c_str(), &guid) ? &guid : nullptr;
	}

	if (auto it = s_Providers.find(name); it != s_Providers.end())
		return &it->second;
	return nullptr;
}

bool TraceSession::AddKernelEventTypes(std::initializer_list<KernelEventTypes> types) {
	m_KernelEventTypes.insert(types);
	return true;
}

bool TraceSession::SetKernelEventTypes(std::initializer_list<KernelEventTypes> types) {
	m_KernelEventTypes = types;
	return true;
}

bool TraceSession::SetKernelEventTypes(std::span<KernelEventTypes const> types) {
	m_KernelEventTypes = std::unordered_set<KernelEventTypes>(types.begin(), types.end());
	return true;
}

std::vector<KernelEventTypes> TraceSession::GetKernelEventTypes() const noexcept {
	return std::vector<KernelEventTypes>(m_KernelEventTypes.begin(), m_KernelEventTypes.end());
}

bool TraceSession::SetKernelEventStacks(std::initializer_list<std::wstring> categories) {
	m_KernelEventStacks = categories;
	return true;
}

bool TraceSession::SetKernelEventStacks(std::span<std::wstring const> categories) {
	m_KernelEventStacks = std::unordered_set<std::wstring>(categories.begin(), categories.end());
	return true;
}

std::vector<std::wstring> TraceSession::GetKernelEventStacks() const {
	return std::vector<std::wstring>(m_KernelEventStacks.begin(), m_KernelEventStacks.end());
}

bool TraceSession::HasStacks() const noexcept {
	if (!m_KernelEventStacks.empty())
		return true;
	for (auto& [guid, config] : m_Providers)
		if (config.Stacks)
			return true;
	return false;
}

bool TraceSession::AddProvider(GUID const& guid, int level, bool stacks) {
	if (m_Providers.contains(guid))
		return true;

	ProviderConfig config;
	config.Level = level;
	config.Stacks = stacks;
	return AddProvider(guid, config);
}

bool TraceSession::AddProvider(GUID const& guid, ProviderConfig const& config) {
	m_Providers.insert_or_assign(guid, config);
	if (m_hTrace) {
		auto ok = EnableProvider(guid, config) == ERROR_SUCCESS;
		UpdateImageTracking();
		return ok;
	}
	return true;
}

bool TraceSession::RemoveProvider(GUID const& guid) {
	if (!m_Providers.erase(guid))
		return false;
	m_EventIds.erase(guid);
	if (m_hTrace) {
		auto ok = ::EnableTraceEx2(m_hTrace, &guid, EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, nullptr) == ERROR_SUCCESS;
		UpdateImageTracking();
		return ok;
	}
	return true;
}

namespace {
	const ULONGLONG KernelProcessImageKeyword = 0x40;	// WINEVENT_KEYWORD_IMAGE
}

void TraceSession::UpdateImageTracking() {
	if (!m_hTrace || m_KernelSession)
		return;		// a kernel session has the Image Load flag (UpdateEventConfig)

	auto images = HasStacks();
	if (auto it = m_Providers.find(KernelProcessProviderGuid); it != m_Providers.end()) {
		// the user's own: EnableProvider adds the image keyword
		if (m_ImageProviderAdded)
			m_ImageProviderAdded = false;
		auto any = it->second.MatchAnyKeyword;
		m_HideImageEvents = images && any != 0 && !(any & KernelProcessImageKeyword);
		EnableProvider(it->first, it->second);
	}
	else if (images) {
		ProviderConfig config{ TRACE_LEVEL_INFORMATION, KernelProcessImageKeyword };
		m_ImageProviderAdded = EnableProvider(KernelProcessProviderGuid, config) == ERROR_SUCCESS;
		m_HideImageEvents = true;
	}
	else {
		if (m_ImageProviderAdded)
			::EnableTraceEx2(m_hTrace, &KernelProcessProviderGuid, EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, nullptr);
		m_ImageProviderAdded = false;
		m_HideImageEvents = false;
	}
}

ULONG TraceSession::EnableProvider(GUID const& guid, ProviderConfig const& cfg) {
	auto config = cfg;
	// stacks: the user's Kernel-Process also delivers image events (see UpdateImageTracking)
	if (guid == KernelProcessProviderGuid && !m_KernelSession && config.MatchAnyKeyword && HasStacks())
		config.MatchAnyKeyword |= 0x40;		// WINEVENT_KEYWORD_IMAGE

	ENABLE_TRACE_PARAMETERS params{ ENABLE_TRACE_PARAMETERS_VERSION_2 };
	if (config.Stacks)
		params.EnableProperty = EVENT_ENABLE_PROPERTY_STACK_TRACE;

	// let ETW drop unselected event IDs, so they never reach the session (or its log file);
	// OnEventRecord still filters, which covers more IDs than the ETW filter can hold
	std::vector<BYTE> buffer;
	EVENT_FILTER_DESCRIPTOR filter{};
	if (auto it = m_EventIds.find(guid); it != m_EventIds.end() && it->second.size() <= MAX_EVENT_FILTER_EVENT_ID_COUNT) {
		auto& ids = it->second;
		buffer.resize(FIELD_OFFSET(EVENT_FILTER_EVENT_ID, Events[ids.size()]));
		auto eventIds = (EVENT_FILTER_EVENT_ID*)buffer.data();
		eventIds->FilterIn = TRUE;
		eventIds->Count = (USHORT)ids.size();
		std::ranges::copy(ids, eventIds->Events);
		filter.Ptr = (ULONGLONG)buffer.data();
		filter.Size = (ULONG)buffer.size();
		filter.Type = EVENT_FILTER_TYPE_EVENT_ID;
		params.EnableFilterDesc = &filter;
		params.FilterDescCount = 1;
	}

	auto error = ::EnableTraceEx2(m_hTrace, &guid, EVENT_CONTROL_CODE_ENABLE_PROVIDER, (UCHAR)config.Level,
		config.MatchAnyKeyword, config.MatchAllKeyword, 0, &params);
	if (error != ERROR_SUCCESS && params.FilterDescCount) {
		// event ID filtering not supported here: filter in OnEventRecord only
		params.EnableFilterDesc = nullptr;
		params.FilterDescCount = 0;
		error = ::EnableTraceEx2(m_hTrace, &guid, EVENT_CONTROL_CODE_ENABLE_PROVIDER, (UCHAR)config.Level,
			config.MatchAnyKeyword, config.MatchAllKeyword, 0, &params);
	}
	return error;
}

bool TraceSession::AddProvider(std::wstring const& name, int level) {
	if (name[0] == L'{') {
		GUID guid;
		if (S_OK != CLSIDFromString(name.c_str(), &guid))
			return false;
		return AddProvider(guid, level);
	}
	if (EnumProviders()) {
		if (auto it = s_Providers.find(name); it != s_Providers.end())
			return AddProvider(it->second, level);
	}
	return false;
}

bool TraceSession::AddEventsForProvider(GUID const& guid, std::span<USHORT const> ids) {
	if (ids.empty())
		m_EventIds.erase(guid);
	else
		m_EventIds.insert_or_assign(guid, std::unordered_set<USHORT>(ids.begin(), ids.end()));
	// a live session: re-enable with the new event ID filter
	if (auto it = m_Providers.find(guid); m_hTrace && it != m_Providers.end())
		return EnableProvider(guid, it->second) == ERROR_SUCCESS;
	return true;
}

std::vector<USHORT> TraceSession::GetEventsForProvider(GUID const& guid) const {
	std::vector<USHORT> ids;
	if (auto it = m_EventIds.find(guid); it != m_EventIds.end())
		ids.assign(it->second.begin(), it->second.end());
	std::ranges::sort(ids);
	return ids;
}

std::vector<std::pair<GUID, ProviderConfig>> TraceSession::GetProviders() const noexcept {
	return std::vector<std::pair<GUID, ProviderConfig>>(m_Providers.begin(), m_Providers.end());
}

void TraceSession::SetOutputFile(std::wstring path) {
	m_OutputFile = std::move(path);
}

std::wstring const& TraceSession::OutputFile() const noexcept {
	return m_OutputFile;
}

void TraceSession::SetBufferSettings(BufferSettings const& settings) noexcept {
	m_Buffers = settings;
}

BufferSettings const& TraceSession::GetBufferSettings() const noexcept {
	return m_Buffers;
}

bool TraceSession::IsKernelSession() const noexcept {
	return m_KernelSession;
}

bool TraceSession::SessionExists(PCWSTR name) {
	constexpr ULONG maxName = 1024;
	auto size = sizeof(EVENT_TRACE_PROPERTIES) + 2 * maxName * sizeof(WCHAR);
	auto buffer = std::make_unique<BYTE[]>(size);
	auto props = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buffer.get());
	props->Wnode.BufferSize = (ULONG)size;
	props->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
	props->LogFileNameOffset = sizeof(EVENT_TRACE_PROPERTIES) + maxName * sizeof(WCHAR);
	return ::ControlTrace(0, name, props, EVENT_TRACE_CONTROL_QUERY) != ERROR_WMI_INSTANCE_NOT_FOUND;
}

bool TraceSession::Start(EventCallback cb, bool cont) {
	m_StoreEvents = false;
	if (cb)
		m_Callback = cb;
	if (m_Callback == nullptr)
		return false;

	return StartCommon(cont);
}

bool TraceSession::Start(EventCallbackNoOwn cb, bool cont) {
	m_StoreEvents = true;
	if (m_Events.empty())
		m_Events.reserve(1 << 18);
	if (cb)
		m_CallbackNoOwn = cb;
	if (m_CallbackNoOwn == nullptr)
		return false;

	return StartCommon(cont);
}

bool TraceSession::Stop(bool quit) noexcept {
	m_Quit = quit;
	m_Stopped = true;
	m_ProcessesTimer.reset();	// cancels and waits for a running refresh
	if (m_hTrace) {
		assert(IsRealTimeSession());
		::ControlTrace(m_hTrace, nullptr, m_Properties, EVENT_TRACE_CONTROL_STOP);
		::CloseTrace(m_hTrace);
		m_hTrace = 0;
	}
	CloseOpenTrace();
	if (m_hProcessThread) {
		// ProcessTrace returns once the current buffer is done; OnEventRecord drops the rest of it,
		// so this is quick. Killing the thread instead could leave a heap or a lock held.
		assert(::GetThreadId(m_hProcessThread.get()) != ::GetCurrentThreadId());
		if (::GetThreadId(m_hProcessThread.get()) == ::GetCurrentThreadId())
			return false;
		::WaitForSingleObject(m_hProcessThread.get(), INFINITE);
		m_hProcessThread.reset();
	}

	return true;
}

void TraceSession::CloseOpenTrace() noexcept {
	if (auto h = m_hOpenTrace.exchange(INVALID_PROCESSTRACE_HANDLE); h != INVALID_PROCESSTRACE_HANDLE)
		::CloseTrace(h);
}

bool TraceSession::IsRunning() const noexcept {
	return m_hOpenTrace != INVALID_PROCESSTRACE_HANDLE;
}

std::wstring TraceSession::GetProcessImageById(DWORD pid) {
	std::shared_lock locker(s_ProcessesLock);
	if (auto it = s_Processes.find(pid); it != s_Processes.end())
		return it->second.ImageName;
	static std::wstring empty;
	return empty;
}

void TraceSession::EnumProcesses() {
	wil::unique_handle hSnapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
	if (!hSnapshot)
		return;

	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(pe);

	if (!::Process32First(hSnapshot.get(), &pe))
		return;

	// build the map without the lock (opening each process is slow), then swap it in
	std::unordered_map<DWORD, ProcessInfo> processes;
	processes.reserve(512);

	do {
		ProcessInfo pi;
		pi.Id = pe.th32ProcessID;
		pi.ImageName = pe.szExeFile;
		pi.FullPath = GetProcessFullPath(pi.Id);
		processes.insert({ pe.th32ProcessID, std::move(pi) });
	} while (::Process32Next(hSnapshot.get(), &pe));

	std::lock_guard locker(s_ProcessesLock);
	s_Processes.swap(processes);
}

namespace {
	DWORD GetDwordProperty(EventData const* data, PCWSTR name) {
		auto prop = data->GetProperty(name);
		return prop && prop->GetLength() == sizeof(DWORD) ? prop->GetValue<DWORD>() : 0;
	}
}

bool TraceSession::ParseProcessStartStop(EventData* data) {
	auto& provider = data->GetProviderId();
	auto& desc = data->GetEventDescriptor();
	if (provider == ProcessGuid) {
		// NT kernel logger (MOF) process events
		switch (desc.Opcode) {
			case 1:		// Start
			case 3:		// DCStart (rundown of existing processes)
				if (auto pid = GetDwordProperty(data, L"ProcessId"); pid) {
					auto prop = data->GetProperty(L"ImageFileName");
					if (auto name = prop ? prop->GetAnsiString() : nullptr; name && *name)
						AddProcessName(pid, std::wstring(name, name + strlen(name)));
				}
				return true;

			case 2:		// End
				if (auto pid = GetDwordProperty(data, L"ProcessId"); pid)
					RemoveProcessName(pid);
				return true;
		}
	}
	else if (provider == KernelProcessProviderGuid) {
		switch (desc.Id) {
			case 1:		// ProcessStart
				if (auto pid = GetDwordProperty(data, L"ProcessID"); pid) {
					auto prop = data->GetProperty(L"ImageName");
					if (auto path = prop ? prop->GetUnicodeString() : nullptr; path && *path) {
						auto name = wcsrchr(path, L'\\');
						AddProcessName(pid, name ? name + 1 : path);
					}
				}
				return true;

			case 2:		// ProcessStop
				if (auto pid = GetDwordProperty(data, L"ProcessID"); pid)
					RemoveProcessName(pid);
				return true;
		}
	}
	return false;
}

namespace {
	ULONG64 GetPointerProperty(EventData const* data, PCWSTR name) {
		auto prop = data->GetProperty(name);
		if (!prop)
			return 0;
		switch (prop->GetLength()) {
			case sizeof(ULONG64): return prop->GetValue<ULONG64>();
			case sizeof(ULONG): return prop->GetValue<ULONG>();
		}
		return 0;
	}
}

bool TraceSession::ParseImageEvent(EventData* data) {
	auto& provider = data->GetProviderId();
	auto& desc = data->GetEventDescriptor();
	enum class Kind { None, Load, Rundown, Unload } kind = Kind::None;
	PCWSTR pidName = L"ProcessId", fileName = L"FileName";
	if (provider == ImageLoadGuid) {
		// NT kernel logger (MOF): Load, DCStart/DCEnd (rundown), Unload
		kind = desc.Opcode == 10 ? Kind::Load : desc.Opcode == 3 || desc.Opcode == 4 ? Kind::Rundown : desc.Opcode == 2 ? Kind::Unload : Kind::None;
	}
	else if (provider == KernelProcessProviderGuid) {
		// Microsoft-Windows-Kernel-Process: ImageLoad, ImageUnload
		kind = desc.Id == 5 ? Kind::Load : desc.Id == 6 ? Kind::Unload : Kind::None;
		pidName = L"ProcessID";
		fileName = L"ImageName";
	}
	if (kind == Kind::None)
		return provider == ImageLoadGuid;	// the kernel's other image events (e.g. KernelBase) count as image events too

	auto base = GetPointerProperty(data, L"ImageBase");
	if (!base)
		return true;
	auto pid = GetDwordProperty(data, pidName);
	auto time = (LONGLONG)data->GetTimeStamp();

	std::lock_guard locker(m_ImagesLock);
	auto& images = m_Images[pid];
	// the image at this base that is still loaded
	auto loaded = std::ranges::find_if(images, [&](auto& image) { return image.Base == base && image.UnloadTime == LLONG_MAX; });
	if (kind == Kind::Unload) {
		if (loaded != images.end())
			loaded->UnloadTime = time;
		return true;
	}
	if (kind == Kind::Rundown && loaded != images.end())
		return true;		// already known (a load, or the other rundown)

	ImageInfo image;
	image.Base = base;
	image.Size = (ULONG)GetPointerProperty(data, L"ImageSize");		// pointer sized
	image.TimeDateStamp = GetDwordProperty(data, L"TimeDateStamp");
	auto name = data->GetProperty(fileName);
	if (auto text = name ? name->GetUnicodeString() : nullptr; text)
		image.FileName = text;
	image.LoadTime = kind == Kind::Rundown ? LLONG_MIN : time;
	if (loaded != images.end())
		loaded->UnloadTime = time;		// a load over a loaded image: the unload was missed
	images.push_back(std::move(image));
	return true;
}

std::optional<ImageInfo> TraceSession::FindImage(DWORD pid, ULONG64 address, LONGLONG time) const {
	std::shared_lock locker(m_ImagesLock);
	auto it = m_Images.find(pid);
	if (it == m_Images.end())
		return {};
	for (auto& image : it->second) {
		if (address >= image.Base && address < image.Base + image.Size && time >= image.LoadTime && time < image.UnloadTime)
			return image;
	}
	return {};
}

void TraceSession::ResetIndex(uint32_t index) {
	m_Index = index;
}

int TraceSession::UpdateEventConfig() {
	typedef struct _PERFINFO_GROUPMASK {
		ULONG Masks[8];
	} PERFINFO_GROUPMASK;

	PERFINFO_GROUPMASK gm{};
	gm.Masks[0] = EVENT_TRACE_FLAG_PROCESS;
	for (auto type : m_KernelEventTypes) {
		gm.Masks[((uint64_t)type) >> 32] |= (ULONG)type;
	}
	// image loads (and the rundown of the running processes' images) symbolize the stacks
	// of processes that exit; shown only when asked for
	auto images = HasStacks();
	if (images)
		gm.Masks[0] |= EVENT_TRACE_FLAG_IMAGE_LOAD;
	m_HideImageEvents = images && !m_KernelEventTypes.contains(KernelEventTypes::ImageLoad);
	auto error = ::TraceSetInformation(m_hTrace, TraceSystemTraceEnableFlagsInfo, &gm, sizeof(gm));
	if (error != ERROR_SUCCESS)
		return error;

	std::vector<CLASSIC_EVENT_ID> stacks;
	stacks.reserve(32);
	for (auto& name : m_KernelEventStacks) {
		auto cat = KernelEventCategory::GetCategory(name.c_str());
		assert(cat);
		for (auto& evt : cat->Events) {
			CLASSIC_EVENT_ID id{};
			id.EventGuid = *cat->Guid;
			id.Type = evt.Opcode;
			stacks.push_back(id);
		}
	}

	error = ::TraceSetInformation(m_hTrace, TraceStackTracingInfo, stacks.data(), (ULONG)stacks.size() * sizeof(CLASSIC_EVENT_ID));
	return error;
}

void TraceSession::OnEventRecord(PEVENT_RECORD rec) {
	if (m_Quit || m_Paused || m_Stopped)
		return;

	if (!IsRealTimeSession()) {
		auto time = rec->EventHeader.TimeStamp.QuadPart;
		if (time == m_FilePos.Time)
			m_FilePos.Count++;
		else
			m_FilePos = { time, 1 };
		// already processed before the pause
		if (m_FilePos.Time < m_ResumePos.Time || (m_FilePos.Time == m_ResumePos.Time && m_FilePos.Count <= m_ResumePos.Count))
			return;
	}

	if (auto it = m_EventIds.find(rec->EventHeader.ProviderId); it != m_EventIds.end()) {
		//
		// check if event should be filtered
		//
		auto id = rec->EventHeader.EventDescriptor.Id;
		if (!it->second.contains(id))
			return;
	}

	if (m_TrackStacks && rec->EventHeader.ProviderId == StackWalkGuid && rec->EventHeader.EventDescriptor.Opcode == 32) {
		AttachKernelStack(rec);
		return;
	}

	if (m_StoreEvents && rec->EventHeader.ProviderId == KernelEventTracingProviderGuid && rec->EventHeader.EventDescriptor.Id == 18) {
		AttachUserStack(rec);	// not an event of its own
		return;
	}

	auto pid = rec->EventHeader.ProcessId;

	auto data = std::make_unique<EventData>(rec, GetProcessImageById(pid), ++m_Index);
	if (::GetLastError() == ERROR_SUCCESS) {
		if (ParseImageEvent(data.get()) && m_HideImageEvents && IsRealTimeSession()) {
			--m_Index;		// enabled only for symbols
			return;
		}
		// process start/stop events update the process names, and are displayed like any other event
		if (!ParseProcessStartStop(data.get()) && (data->GetProcessId() == 0 || data->GetProcessId() == (DWORD)-1)) {
			HandleNoProcessId(data.get());
		}
	}
	if (m_Callback || m_CallbackNoOwn) {
		{
			std::lock_guard locker(m_FilterLock);
			if (m_FilterMgr.Eval(data.get()) == FilterResult::Exclude)
				return;
		}

		if (m_StoreEvents) {
			auto evt = data.get();
			std::lock_guard locker(m_EventsLock);
			m_Events.push_back(std::move(data));
			if (auto matchId = evt->GetStackMatchId(); matchId) {
				m_PendingUserStacks[matchId] = evt;
				m_PendingUserStackOrder.push_back(matchId);
				// user parts that never come (e.g. the thread never returned to user mode)
				if (m_PendingUserStackOrder.size() > 4096) {
					m_PendingUserStacks.erase(m_PendingUserStackOrder.front());
					m_PendingUserStackOrder.pop_front();
				}
			}
			if (m_TrackStacks) {
				auto& history = m_RecentByThread[evt->GetThreadId()];
				history.push_back({ evt });
				if (history.size() > 16)
					history.pop_front();
			}
			m_CallbackNoOwn(evt);
		}
		else {
			m_Callback(std::move(data));
		}
	}
	else {
		--m_Index;
	}
}

DWORD TraceSession::Run() {
	assert(m_hTrace != INVALID_PROCESSTRACE_HANDLE);
	DWORD err;
	TRACEHANDLE hTrace = m_hOpenTrace;
	if (m_LogFileName.empty()) {
		EnumProcesses();
		FILETIME now;
		::GetSystemTimeAsFileTime(&now);
		err = ::ProcessTrace(&hTrace, 1, &now, nullptr);
	}
	else {
		FILETIME ft;
		PFILETIME start = nullptr;
		if (m_ResumePos.Count) {
			// ProcessTrace skips earlier events; OnEventRecord skips the ones at this time already seen
			*(LONGLONG*)&ft = m_ResumePos.Time;
			start = &ft;
		}
		err = ::ProcessTrace(&hTrace, 1, start, nullptr);
	}
	CloseOpenTrace();
	// ProcessTrace also succeeds when CloseTrace cut it short
	if (err == ERROR_SUCCESS && !m_Stopped)
		m_Done = true;
	return err;
}

void TraceSession::SetLogFileName(PCWSTR path) {
	m_LogFileName = path;
}

void TraceSession::HandleNoProcessId(EventData* data) {
	DWORD tid = 0;
	if (data->GetThreadId() == 0 || data->GetThreadId() == (DWORD)-1) {
		auto prop = data->GetProperty(L"ThreadId");
		if (!prop)
			prop = data->GetProperty(L"TThreadId");
		if (prop) {
			tid = prop->GetValue<DWORD>();
			data->m_ThreadId = tid;
		}
	}
	if (data->GetProcessId() == 0 || data->GetProcessId() == (DWORD)-1) {
		auto prop = data->GetProperty(L"PID");
		if (prop == nullptr)
			prop = data->GetProperty(L"ProcessId");
		if (prop) {
			auto pid = prop->GetValue<DWORD>();
			data->m_ProcessId = pid;
			data->SetProcessName(GetProcessImageById(pid));
		}
		else if (tid) {
			wil::unique_handle hThread(::OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid));
			if (hThread) {
				auto pid = ::GetProcessIdOfThread(hThread.get());
				if (pid) {
					data->m_ProcessId = pid;
					data->SetProcessName(GetProcessImageById(pid));
				}
			}
		}
	}
}

int64_t TraceSession::RawToEventTime(ULONG64 raw) const noexcept {
	// exact integer math (FILETIME values exceed double precision); split to avoid overflow
	constexpr ULONG64 units = 10000000;
	auto freq = m_StackTimeFrequency;
	return (int64_t)(raw / freq * units + raw % freq * units / freq);
}

void TraceSession::AttachUserStack(PEVENT_RECORD rec) {
	ULONG64 matchId = 0;
	std::vector<ULONG64> frames;
	for (USHORT i = 0; i < rec->ExtendedDataCount; i++) {
		auto& item = rec->ExtendedData[i];
		if (item.ExtType == EVENT_HEADER_EXT_TYPE_STACK_TRACE64) {
			auto stack = reinterpret_cast<PEVENT_EXTENDED_ITEM_STACK_TRACE64>(item.DataPtr);
			matchId = stack->MatchId;
			frames.assign(stack->Address, stack->Address + (item.DataSize - sizeof(ULONG64)) / sizeof(ULONG64));
		}
		else if (item.ExtType == EVENT_HEADER_EXT_TYPE_STACK_TRACE32) {
			auto stack = reinterpret_cast<PEVENT_EXTENDED_ITEM_STACK_TRACE32>(item.DataPtr);
			matchId = stack->MatchId;
			frames.assign(stack->Address, stack->Address + (item.DataSize - sizeof(ULONG64)) / sizeof(ULONG));
		}
	}
	if (matchId == 0 || frames.empty())
		return;

	// the owner may have been filtered out: then there is nothing to attach to
	std::lock_guard locker(m_EventsLock);
	if (auto it = m_PendingUserStacks.find(matchId); it != m_PendingUserStacks.end()) {
		it->second->AppendStackTrace(frames);
		m_PendingUserStacks.erase(it);
	}
}

void TraceSession::AttachKernelStack(PEVENT_RECORD rec) {
	//
	// The kernel logger reports stacks as separate StackWalk events. Payload: EventTimeStamp (UINT64),
	// StackProcess, StackThread, then addresses. EventTimeStamp is the owner event's raw timestamp,
	// while header times are converted, so match via a linear conversion whose offset is learned from
	// the common case of a stack immediately following its event. Notes:
	// - some stacks arrive after later events of the same thread (hence the per-thread history)
	// - a stack may be split into two StackWalk events (kernel part, user part) with the same EventTimeStamp
	//
	constexpr USHORT headerSize = sizeof(ULONG64) + 2 * sizeof(ULONG);
	if (rec->UserDataLength <= headerSize)
		return;

	auto data = static_cast<const BYTE*>(rec->UserData);
	ULONG64 timeStamp;
	::memcpy(&timeStamp, data, sizeof(timeStamp));
	ULONG tid;
	::memcpy(&tid, data + sizeof(ULONG64) + sizeof(ULONG), sizeof(tid));

	std::vector<ULONG64> stack;
	auto frames = data + headerSize;
	if (rec->EventHeader.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER) {
		auto count = (rec->UserDataLength - headerSize) / sizeof(ULONG);
		stack.resize(count);
		for (size_t i = 0; i < count; i++) {
			ULONG address;
			::memcpy(&address, frames + i * sizeof(ULONG), sizeof(address));
			stack[i] = address;
		}
	}
	else {
		auto count = (rec->UserDataLength - headerSize) / sizeof(ULONG64);
		stack.resize(count);
		::memcpy(stack.data(), frames, count * sizeof(ULONG64));
	}

	std::lock_guard locker(m_EventsLock);
	auto it = m_RecentByThread.find(tid);
	if (it == m_RecentByThread.end() || it->second.empty())
		return;

	auto& history = it->second;
	StackOwner* owner = nullptr;
	constexpr int CalibrationSamples = 3;
	// conversion rounding
	constexpr int64_t Tolerance = 2;
	if (m_StackTimeSamples >= CalibrationSamples) {
		auto expected = RawToEventTime(timeStamp) + m_StackTimeOffset;
		for (auto& candidate : history) {
			if (std::abs((int64_t)candidate.Event->GetTimeStamp() - expected) <= Tolerance) {
				owner = &candidate;
				break;
			}
		}
	}
	else {
		// not calibrated yet: assume the stack belongs to the latest event of the thread
		auto& last = history.back();
		if (last.Event->HasStackTrace() && last.StackTimeStamp != timeStamp)
			return;
		owner = &last;
		if (!last.Event->HasStackTrace()) {
			auto offset = (int64_t)last.Event->GetTimeStamp() - RawToEventTime(timeStamp);
			if (m_StackTimeSamples > 0 && std::abs(offset - m_StackTimeOffset) <= Tolerance)
				m_StackTimeSamples++;
			else {
				m_StackTimeOffset = offset;
				m_StackTimeSamples = 1;
			}
		}
	}
	if (!owner)
		return;

	if (!owner->Event->HasStackTrace())
		owner->Event->SetStackTrace(std::move(stack));
	else if (owner->StackTimeStamp == timeStamp)
		owner->Event->AppendStackTrace(stack);
	else
		return;
	owner->StackTimeStamp = timeStamp;
}

bool TraceSession::EnumProviders() {
	if (!s_Providers.empty())
		return true;

	DWORD size = 1 << 17;
	wil::unique_virtualalloc_ptr<PROVIDER_ENUMERATION_INFO> buffer((PPROVIDER_ENUMERATION_INFO)::VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	if (!buffer)
		return false;

	if (ERROR_SUCCESS != ::TdhEnumerateProviders(buffer.get(), &size))
		return false;

	for (DWORD i = 0; i < buffer->NumberOfProviders; i++) {
		std::wstring name((PCWSTR)((PBYTE)buffer.get() + buffer->TraceProviderInfoArray[i].ProviderNameOffset));
		s_Providers.insert({ name, buffer->TraceProviderInfoArray[i].ProviderGuid });
	}
	return true;
}

bool TraceSession::StartCommon(bool cont) {
	m_Continue = cont;
	if (cont && m_Done)
		return true;	// nothing left to resume
	m_ResumePos = cont ? m_FilePos : FilePosition{};
	if (!cont) {
		// a log file read again from the start sees its image events again
		std::lock_guard locker(m_ImagesLock);
		m_Images.clear();
	}
	m_FilePos = {};
	m_Done = false;
	m_Stopped = false;
	// log files may contain StackWalk events; real-time sessions only when stacks were requested
	m_TrackStacks = m_StoreEvents && (!IsRealTimeSession() || !m_KernelEventStacks.empty());

	//error = UpdateEventConfig();
	//if (error != ERROR_SUCCESS) {
	//	Stop();
	//	return false;
	//}

	m_TraceLog.Context = this;
	m_TraceLog.LoggerName = m_SessionName.data();
	if (!m_LogFileName.empty())
		m_TraceLog.LogFileName = m_LogFileName.data();
	m_TraceLog.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD | (IsRealTimeSession() ? PROCESS_TRACE_MODE_REAL_TIME : 0);
	//m_TraceLog.BufferCallback = [](auto etl) -> ULONG {
	//	return ((TraceSession*)etl->Context)->IsRunning();
	//};

	m_TraceLog.EventRecordCallback = [](PEVENT_RECORD record) {
		auto session = ((TraceSession*)record->UserContext);
		session->OnEventRecord(record);
	};
	m_hOpenTrace = ::OpenTrace(&m_TraceLog);
	if (m_hOpenTrace == INVALID_PROCESSTRACE_HANDLE)
		return false;

	// scale from the raw clock (as in StackWalk's EventTimeStamp) to 100nsec units
	LARGE_INTEGER freq{};
	if (IsRealTimeSession())
		::QueryPerformanceFrequency(&freq);		// session clock is QPC (ClientContext == 1)
	else if (m_TraceLog.LogfileHeader.ReservedFlags == 1)
		freq = m_TraceLog.LogfileHeader.PerfFreq;
	else if (m_TraceLog.LogfileHeader.ReservedFlags == 3)
		freq.QuadPart = m_TraceLog.LogfileHeader.CpuSpeedInMHz * 1000000LL;
	m_StackTimeFrequency = freq.QuadPart > 0 ? freq.QuadPart : 10000000;
	m_StackTimeSamples = 0;

	// create a dedicated thread to process the trace
	m_hProcessThread.reset(::CreateThread(nullptr, 0, [](auto param) {
		return ((TraceSession*)param)->Run();
	}, this, 0, nullptr));
	if (!m_hProcessThread) {
		CloseOpenTrace();
		return false;
	}
	::SetThreadPriority(m_hProcessThread.get(), THREAD_PRIORITY_HIGHEST);
	::SetThreadDescription(m_hProcessThread.get(), L"ETW Processing Thread");
	if (::WaitForSingleObject(m_hProcessThread.get(), 100) == WAIT_OBJECT_0) {
		// a small log file may already be fully processed; only an error exit code is a failure
		DWORD code = ERROR_GEN_FAILURE;
		::GetExitCodeThread(m_hProcessThread.get(), &code);
		if (code == ERROR_SUCCESS)
			return true;
		// Run closed the consumer handle; m_hTrace is the session itself, which Stop shuts down
		m_hProcessThread.reset();
		return false;
	}

	if (IsRealTimeSession()) {
		// refresh the process names off the trace thread (process start/stop events only arrive for kernel sessions)
		m_ProcessesTimer.reset(::CreateThreadpoolTimer([](PTP_CALLBACK_INSTANCE, PVOID, PTP_TIMER) { EnumProcesses(); }, nullptr, nullptr));
		if (m_ProcessesTimer) {
			LARGE_INTEGER due;
			due.QuadPart = -5 * 10000000LL;
			::SetThreadpoolTimer(m_ProcessesTimer.get(), (PFILETIME)&due, 5000, 1000);
		}
	}

	return true;
}

std::wstring TraceSession::GetProcessFullPath(DWORD pid) {
	WCHAR path[MAX_PATH];
	BOOL ok = FALSE;
	if (auto hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid); hProcess) {
		DWORD size = _countof(path);
		ok = ::QueryFullProcessImageName(hProcess, 0, path, &size);
		::CloseHandle(hProcess);
	}
	return ok ? path : L"";
}

void TraceSession::AddProcessName(DWORD pid, std::wstring name) {
	ProcessInfo pi;
	pi.Id = pid;
	pi.ImageName = std::move(name);
	pi.FullPath = GetProcessFullPath(pid);
	std::lock_guard locker(s_ProcessesLock);
	s_Processes.insert_or_assign(pid, std::move(pi));	// process IDs are reused
}

bool TraceSession::RemoveProcessName(DWORD pid) {
	std::lock_guard locker(s_ProcessesLock);
	return s_Processes.erase(pid);
}

std::wstring TraceSession::GetDosNameFromNtName(PCWSTR name) {
	static std::vector<std::pair<std::wstring, std::wstring>> deviceNames;
	static bool first = true;
	if (first) {
		auto drives = ::GetLogicalDrives();
		int drive = 0;
		while (drives) {
			if (drives & 1) {
				// drive exists
				WCHAR driveName[] = L"X:";
				driveName[0] = (WCHAR)(drive + 'A');
				WCHAR path[MAX_PATH];
				if (::QueryDosDevice(driveName, path, MAX_PATH)) {
					deviceNames.push_back({ path, driveName });
				}
			}
			drive++;
			drives >>= 1;
		}
		first = false;
	}

	for (auto& [ntName, dosName] : deviceNames) {
		if (::_wcsnicmp(name, ntName.c_str(), ntName.size()) == 0)
			return dosName + (name + ntName.size());
	}
	return L"";
}

void TraceSession::Pause(bool pause) noexcept {
	// for log files too: events delivered while ProcessTrace winds down are dropped, and replayed on resume
	m_Paused = pause;
	if(!m_LogFileName.empty()) {
		if (pause)
			Stop();
		else {
			m_StoreEvents ? Start(m_CallbackNoOwn, !m_LogFileName.empty()) : Start(m_Callback, !m_LogFileName.empty());
		}
	}
}

bool TraceSession::IsRealTimeSession() const noexcept {
	return m_RealTimeSession;
}

bool TraceSession::Init() {
	if (m_hTrace)
		return true;

	auto nameSize = (m_SessionName.length() + 1) * sizeof(WCHAR);
	auto size = sizeof(EVENT_TRACE_PROPERTIES) + nameSize + (m_OutputFile.length() + 1) * sizeof(WCHAR);
	m_PropertiesBuffer = std::make_unique<BYTE[]>(size);
	ULONG error;
	m_RealTimeSession = true;
	bool systemLogger = !m_KernelEventTypes.empty();
	m_KernelSession = systemLogger;

	for (;;) {
		::memset(m_PropertiesBuffer.get(), 0, size);

		m_Properties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(m_PropertiesBuffer.get());
		//m_Properties->EnableFlags = (ULONG)KernelEventTypes::Process;
		m_Properties->Wnode.BufferSize = (ULONG)size;
		//m_Properties->Wnode.Guid = sessionGuid;
		//m_Properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
		m_Properties->Wnode.ClientContext = 1;
		m_Properties->FlushTimer = m_Buffers.FlushTimerSec;
		m_Properties->BufferSize = m_Buffers.BufferSizeKB;
		m_Properties->MinimumBuffers = m_Buffers.MinimumBuffers;
		m_Properties->MaximumBuffers = m_Buffers.MaximumBuffers;
		m_Properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE | (systemLogger ? EVENT_TRACE_SYSTEM_LOGGER_MODE : 0);
		m_Properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
		if (!m_OutputFile.empty()) {
			// also write the session to a file
			m_Properties->LogFileMode |= EVENT_TRACE_FILE_MODE_SEQUENTIAL;
			m_Properties->LogFileNameOffset = (ULONG)(sizeof(EVENT_TRACE_PROPERTIES) + nameSize);
			::memcpy(m_PropertiesBuffer.get() + m_Properties->LogFileNameOffset, m_OutputFile.c_str(), (m_OutputFile.length() + 1) * sizeof(WCHAR));
		}

		error = ::StartTrace(&m_hTrace, m_SessionName.c_str(), m_Properties);
		if (error == ERROR_ALREADY_EXISTS) {
			error = ::ControlTrace(0, m_SessionName.c_str(), m_Properties, EVENT_TRACE_CONTROL_STOP);
			if (error != ERROR_SUCCESS)
				return false;
			continue;
		}
		if (error != ERROR_SUCCESS)
			break;
		if (systemLogger) {
			error = (ULONG)UpdateEventConfig();
			if (error != ERROR_SUCCESS)
				break;
		}
		for (auto& [p, config] : m_Providers) {
			error = EnableProvider(p, config);
			if (error != ERROR_SUCCESS)
				break;
		}
		if (error == ERROR_SUCCESS)
			UpdateImageTracking();
		break;
	}
	return error == ERROR_SUCCESS;
}

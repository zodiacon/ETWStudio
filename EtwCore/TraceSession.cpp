#include "pch.h"
#include <initguid.h>
#include "TraceSession.h"
#include "EventData.h"
#include <assert.h>

#pragma comment(lib, "tdh")

TraceSession::TraceSession(std::wstring_view name) : m_SessionName(name.data()) {
}


TraceSession::~TraceSession() {
	Stop();
}

std::wstring const& TraceSession::SessionName() const {
	return m_SessionName;
}

FilterManager& TraceSession::GetFilterManager() {
	return m_FilterMgr;
}

bool TraceSession::SetSessionName(std::wstring name) {
	if (!m_IsPaused)
		return false;

	m_SessionName = name;
	return true;
}

GUID const* TraceSession::GetProviderGuid(std::wstring const& name) {
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

bool TraceSession::SetKernelEventStacks(std::initializer_list<std::wstring> categories) {
	m_KernelEventStacks = categories;
	return true;
}

bool TraceSession::AddProvider(GUID const& guid, int level) {
	if (m_Providers.contains(guid))
		return true;

	m_Providers.insert({ guid, level });
	if (m_hTrace) {
		auto error = ::EnableTraceEx(&guid, nullptr, m_hTrace, TRUE, (UCHAR)level, 0, 0, 0, nullptr);
		return error == ERROR_SUCCESS;
	}
	return true;
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

bool TraceSession::AddEventsForProvider(GUID const& guid, std::span<USHORT> ids) {
	std::unordered_set<USHORT> events;
	for (auto& id : ids)
		events.insert(id);
	m_EventIds.insert({ guid, std::move(events) });

	return true;
}

std::vector<std::pair< const GUID, int>> TraceSession::GetProviders() const {
	return std::vector(m_Providers.begin(), m_Providers.end());
}

bool TraceSession::Start(EventCallback cb) {
	m_Callback = cb;

	//error = UpdateEventConfig();
	//if (error != ERROR_SUCCESS) {
	//	Stop();
	//	return false;
	//}

	m_TraceLog.Context = this;
	m_TraceLog.LoggerName = m_SessionName.data();
	m_TraceLog.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_REAL_TIME;
	m_TraceLog.EventRecordCallback = [](PEVENT_RECORD record) {
		((TraceSession*)record->UserContext)->OnEventRecord(record);
	};
	m_hOpenTrace = ::OpenTrace(&m_TraceLog);
	if (!m_hOpenTrace)
		return false;

	// create a dedicated thread to process the trace
	m_hProcessThread.reset(::CreateThread(nullptr, 0, [](auto param) {
		return ((TraceSession*)param)->Run();
		}, this, 0, nullptr));
	::SetThreadPriority(m_hProcessThread.get(), THREAD_PRIORITY_HIGHEST);

	return true;
}

bool TraceSession::Stop() {
	if (m_hTrace) {
		::ControlTrace(m_hTrace, KERNEL_LOGGER_NAME, m_Properties, EVENT_TRACE_CONTROL_STOP);
		m_hTrace = 0;
	}
	if (m_hOpenTrace) {
		::CloseTrace(m_hOpenTrace);
		m_hOpenTrace = 0;
	}
	if (WAIT_TIMEOUT == ::WaitForSingleObject(m_hProcessThread.get(), 1000))
		::TerminateThread(m_hProcessThread.get(), 1);
	m_hProcessThread.reset();

	return true;
}

bool TraceSession::IsRunning() const {
	return m_hProcessThread != nullptr;
}

std::wstring const& TraceSession::GetProcessImageById(DWORD pid) const {
	std::shared_lock locker(m_ProcessesLock);
	if (auto it = m_Processes.find(pid); it != m_Processes.end())
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

	m_Processes.clear();
	m_Processes.reserve(512);

	while (::Process32Next(hSnapshot.get(), &pe)) {
		ProcessInfo pi;
		pi.Id = pe.th32ProcessID;
		pi.ImageName = pe.szExeFile;
		pi.FullPath = GetProcessFullPath(pi.Id);
		m_Processes.insert({ pe.th32ProcessID, std::move(pi) });
	}
}

bool TraceSession::ParseProcessStartStop(EventData* data) {
	if (data->GetEventStrings().Task != L"Process")
		return false;

	switch (data->GetEventDescriptor().Opcode) {
		case 1:		// process created
		{
			auto prop = data->GetProperty(L"ImageFileName");
			if (prop) {
				auto name = prop->GetAnsiString();
				if (name) {
					std::wstring pname;
					pname.assign(name, name + strlen(name));
					AddProcessName(data->GetProperty(L"ProcessId")->GetValue<DWORD>(), pname);
					assert(!pname.empty());
					//data->SetProcessName(pname);
				}
			}
			break;
		}

		case 12:		// process end
			RemoveProcessName(data->GetProcessId());
			break;

	}

	return true;
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
	if (m_hOpenTrace == 0 || m_IsPaused)
		return;

	if (auto it = m_EventIds.find(rec->EventHeader.ProviderId); it != m_EventIds.end()) {
		//
		// check if event should be filtered
		//
		auto id = rec->EventHeader.EventDescriptor.Id;
		if (!it->second.contains(id))
			return;
	}

	auto pid = rec->EventHeader.ProcessId;

	std::shared_ptr<EventData> data(new EventData(rec, GetProcessImageById(pid), ++m_Index));
	bool processEvent = true;
	if (::GetLastError() == ERROR_SUCCESS) {
		processEvent = ParseProcessStartStop(data.get());

		if (!processEvent && data->GetProcessId() == 0 || data->GetProcessId() == (DWORD)-1) {
			HandleNoProcessId(data.get());
		}

		m_LastEvent = data;
	}
	if (m_Callback && (!processEvent || m_IsTraceProcesses)) {
		if (m_FilterMgr.Eval(data.get()) == FilterResult::Exclude)
			return;

		m_Callback(data);
	}
	else {
		--m_Index;
	}
}

DWORD TraceSession::Run() {
	EnumProcesses();
	FILETIME now;
	::GetSystemTimeAsFileTime(&now);
	auto error = ::ProcessTrace(&m_hOpenTrace, 1, &now, nullptr);
	return error;
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
	std::lock_guard locker(m_ProcessesLock);
	ProcessInfo pi;
	pi.Id = pid;
	pi.ImageName = std::move(name);
	pi.FullPath = GetProcessFullPath(pid);
	m_Processes.insert({ pid, std::move(pi) });
}

bool TraceSession::RemoveProcessName(DWORD pid) {
	std::lock_guard locker(m_ProcessesLock);
	return m_Processes.erase(pid);
}

bool TraceSession::IsPaused() const {
	return m_IsPaused;
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

bool TraceSession::SetBackupFile(PCWSTR path) {
	if (path) {
		wil::unique_hfile hFile(::CreateFile(path, GENERIC_WRITE | GENERIC_READ, 0, nullptr, TRUNCATE_EXISTING, 0, nullptr));
		if (!hFile)
			return false;
		m_hMemMap.reset(::CreateFileMapping(hFile.get(), nullptr, PAGE_READWRITE | MEM_RESERVE, 32, 0, nullptr));
		if (!m_hMemMap)
			return false;
	}
	return true;
}

void TraceSession::Pause(bool pause) {
	m_IsPaused = pause;
}

bool TraceSession::Init() {
	if (m_hTrace)
		return true;

	// {6990501B-4484-4EF0-8793-84159B8D4728}
	GUID sessionGuid;
	::CoCreateGuid(&sessionGuid);

	auto size = sizeof(EVENT_TRACE_PROPERTIES) + (m_SessionName.length() + 1) * sizeof(WCHAR);
	m_PropertiesBuffer = std::make_unique<BYTE[]>(size);
	ULONG error;

	for (;;) {
		::memset(m_PropertiesBuffer.get(), 0, size);

		m_Properties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(m_PropertiesBuffer.get());
		m_Properties->EnableFlags = (ULONG)KernelEventTypes::Process;
		m_Properties->Wnode.BufferSize = (ULONG)size;
		m_Properties->Wnode.Guid = sessionGuid;
		m_Properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
		m_Properties->Wnode.ClientContext = 1;
		m_Properties->FlushTimer = 1;
		m_Properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE | EVENT_TRACE_SYSTEM_LOGGER_MODE;
		m_Properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

		// copy session name
		::wcscpy_s((PWSTR)(m_Properties + 1), m_SessionName.length() + 1, m_SessionName.c_str());

		error = ::StartTrace(&m_hTrace, m_SessionName.c_str(), m_Properties);
		if (error == ERROR_ALREADY_EXISTS) {
			error = ::ControlTrace(m_hTrace, m_SessionName.c_str(), m_Properties, EVENT_TRACE_CONTROL_STOP);
			if (error != ERROR_SUCCESS)
				return false;
			continue;
		}
		break;
	}
	return error == ERROR_SUCCESS;
}

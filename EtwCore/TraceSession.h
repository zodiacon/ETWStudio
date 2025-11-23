#pragma once

#include "KernelEvents.h"
#include <FilterManager.h>

class EventData;

using EventCallback = std::function<void(std::shared_ptr<EventData>)>;

namespace std {
	template<>
	struct hash<GUID> {
		size_t operator()(GUID const& guid) const {
			return guid.Data1 ^ guid.Data3 * guid.Data3 + guid.Data4[1];
		}
	};
}

class TraceSession final {
public:
	explicit TraceSession(std::wstring name = L"");
	bool OpenFile(PCWSTR path, EventCallback cb);
	~TraceSession();
	TraceSession(const TraceSession&) = delete;
	TraceSession& operator=(const TraceSession&) = delete;
	TraceSession(TraceSession&& other) = default;
	TraceSession& operator=(TraceSession&&) = default;

	FilterManager& GetFilterManager();

	bool SetSessionName(std::wstring name);
	std::wstring const& SessionName() const noexcept;
	std::wstring const& LogFileName() const noexcept;

	static GUID const* GetProviderGuid(std::wstring const& name);

	bool AddKernelEventTypes(std::initializer_list<KernelEventTypes> types);
	bool SetKernelEventTypes(std::initializer_list<KernelEventTypes> types);
	bool SetKernelEventStacks(std::initializer_list<std::wstring> categories);
	bool AddProvider(GUID const& guid, int level = TRACE_LEVEL_INFORMATION);
	bool AddProvider(std::wstring const& name, int level = TRACE_LEVEL_INFORMATION);
	bool AddEventsForProvider(GUID const& guid, std::span<USHORT> ids);

	std::vector<std::pair<const GUID, int>> GetProviders() const;

	bool SetBackupFile(PCWSTR path);
	void Pause(bool pause) noexcept;
	bool IsRealTimeSession() const noexcept;
	bool Init();
	bool Start(EventCallback callback, bool cont = false);
	bool Stop(bool quit = false) noexcept;
	bool IsRunning() const noexcept;

	void ResetIndex(uint32_t index = 0);
	int UpdateEventConfig();

	std::wstring const& GetProcessImageById(DWORD pid) const;
	static std::wstring GetDosNameFromNtName(PCWSTR name);

private:
	static std::wstring GetProcessFullPath(DWORD pid);
	void AddProcessName(DWORD pid, std::wstring name);
	bool RemoveProcessName(DWORD pid);
	static void EnumProcesses();
	bool ParseProcessStartStop(EventData* data);
	void OnEventRecord(PEVENT_RECORD rec);
	DWORD Run();
	void HandleNoProcessId(EventData* data);

private:
	static bool EnumProviders();

	struct ProcessInfo {
		DWORD Id;
		std::wstring ImageName;
		std::wstring FullPath;
	};

	//TRACEHANDLE m_hOpenTrace{ 0 };
	TRACEHANDLE m_hTrace{ 0 };
	TRACEHANDLE m_hOpenTrace{ INVALID_PROCESSTRACE_HANDLE };
	EVENT_TRACE_PROPERTIES* m_Properties;
	std::unique_ptr<BYTE[]> m_PropertiesBuffer;
	EVENT_TRACE_LOGFILE m_TraceLog = { 0 };
	wil::unique_handle m_hProcessThread;
	EventCallback m_Callback;
	std::unordered_set<KernelEventTypes> m_KernelEventTypes;
	std::unordered_set<std::wstring> m_KernelEventStacks;
	mutable std::shared_mutex m_ProcessesLock;
	inline static std::unordered_map<DWORD, ProcessInfo> m_Processes;
	mutable std::unordered_map<ULONGLONG, std::wstring> m_KernelEventNames;
	std::vector<DWORD> m_CleanupPids;
	std::shared_ptr<EventData> m_LastEvent;
	std::shared_ptr<EventData> m_LastExcluded;
	uint32_t m_Index{ 0 };
	wil::unique_handle m_hMemMap;
	bool m_IsTraceProcesses{ false };
	bool m_DumpUnnamedEvents{ false };
	inline static std::unordered_map<std::wstring, GUID> s_Providers;
	std::unordered_map<GUID, std::unordered_set<USHORT>> m_EventIds;
	std::unordered_map<GUID, int> m_Providers;
	std::wstring m_LogFileName;
	std::wstring m_SessionName;
	FilterManager m_FilterMgr;
	TRACE_LOGFILE_HEADER m_LogFileHeader;
	std::atomic<bool> m_Paused{ false };
	bool m_Continue{ false };
	bool m_Quit{ false };
};


#pragma once

#include "KernelEvents.h"
#include <span>

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

class TraceManager final {
public:
	TraceManager();
	~TraceManager();
	TraceManager(const TraceManager&) = delete;
	TraceManager& operator=(const TraceManager&) = delete;

	static GUID const* GetProviderGuid(std::wstring const& name);

	bool AddKernelEventTypes(std::initializer_list<KernelEventTypes> types);
	bool SetKernelEventTypes(std::initializer_list<KernelEventTypes> types);
	bool SetKernelEventStacks(std::initializer_list<std::wstring> categories);
	bool AddProvider(GUID const& guid, int level = TRACE_LEVEL_INFORMATION);
	bool AddProvider(std::wstring const& name, int level = TRACE_LEVEL_INFORMATION);
	bool AddEventsForProvider(GUID const& guid, std::span<USHORT> ids);

	bool SetBackupFile(PCWSTR path);
	void Pause(bool pause);
	bool Init();
	bool Start(EventCallback callback);
	bool Stop();
	bool IsRunning() const;
	bool IsPaused() const;

	void ResetIndex(uint32_t index = 0);
	int UpdateEventConfig();

	std::wstring GetProcessImageById(DWORD pid) const;
	static std::wstring GetDosNameFromNtName(PCWSTR name);

private:
	const std::wstring& GetkernelEventName(EVENT_RECORD* rec) const;
	void AddProcessName(DWORD pid, std::wstring name);
	bool RemoveProcessName(DWORD pid);
	void EnumProcesses();
	bool ParseProcessStartStop(EventData* data);
	void OnEventRecord(PEVENT_RECORD rec);
	DWORD Run();
	void HandleNoProcessId(EventData* data);

private:
	static bool EnumProviders();

	struct ProcessInfo {
		DWORD Id;
		std::wstring ImageName;
		LONGLONG CreateTime;
	};

	TRACEHANDLE m_hOpenTrace{ 0 };
	TRACEHANDLE m_hTrace{ 0 };
	EVENT_TRACE_PROPERTIES* m_Properties;
	std::unique_ptr<BYTE[]> m_PropertiesBuffer;
	EVENT_TRACE_LOGFILE m_TraceLog = { 0 };
	wil::unique_handle m_hProcessThread;
	EventCallback m_Callback;
	std::unordered_set<KernelEventTypes> m_KernelEventTypes;
	std::unordered_set<std::wstring> m_KernelEventStacks;
	mutable std::shared_mutex m_ProcessesLock;
	std::unordered_map<DWORD, std::wstring> m_Processes;
	mutable std::unordered_map<ULONGLONG, std::wstring> m_KernelEventNames;
	std::vector<DWORD> m_CleanupPids;
	std::shared_ptr<EventData> m_LastEvent;
	std::shared_ptr<EventData> m_LastExcluded;
	uint32_t m_Index{ 0 };
	wil::unique_handle m_hMemMap;
	bool m_IsTraceProcesses{ true };
	bool m_DumpUnnamedEvents{ true };
	std::atomic<bool> m_IsPaused{ false };
	inline static std::unordered_map<std::wstring, GUID> s_Providers;
	std::unordered_map<GUID, std::unordered_set<USHORT>> m_EventIds;
};


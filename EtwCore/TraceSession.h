#pragma once

#include "KernelEvents.h"
#include <FilterManager.h>
#include <deque>

class EventData;

using EventCallback = std::function<void(std::unique_ptr<EventData>)>;

// how a (non-kernel) provider is enabled in a session
struct ProviderConfig {
	int Level{ TRACE_LEVEL_INFORMATION };
	ULONGLONG MatchAnyKeyword{ 0 };	// 0: all keywords
	ULONGLONG MatchAllKeyword{ 0 };
	bool Stacks{ false };
};

// zero: system default
struct BufferSettings {
	ULONG BufferSizeKB{ 0 };
	ULONG MinimumBuffers{ 0 };
	ULONG MaximumBuffers{ 0 };
	ULONG FlushTimerSec{ 1 };
};
using EventCallbackNoOwn = std::function<void(EventData*)>;

namespace std {
	template<>
	struct hash<GUID> {
		size_t operator()(GUID const& guid) const {
			return guid.Data1 ^ guid.Data3 * guid.Data2 + guid.Data4[1];
		}
	};
}

class TraceSession final {
public:
	explicit TraceSession(std::wstring name = L"");
	bool OpenFile(PCWSTR path, EventCallback cb);
	bool OpenFile(PCWSTR path, EventCallbackNoOwn cb);
	~TraceSession();

	std::vector<std::unique_ptr<EventData>> const& GetEvents() const noexcept {
		return m_Events;
	}
	// copies the stored event pointers; onSnapshot runs under the events lock (as with ClearEvents).
	// The pointers stay valid until ClearEvents or destruction.
	std::vector<EventData*> SnapshotEvents(std::function<void()> const& onSnapshot = nullptr);

	FilterManager& GetFilterManager() noexcept;
	// safe to call while the session is running
	void SetFilterManager(FilterManager mgr);
	// deletes all stored events; onClear runs under the events lock, so the caller can drop
	// any borrowed pointers it holds without racing the trace thread
	void ClearEvents(std::function<void()> const& onClear = nullptr);

	bool SetSessionName(std::wstring name) noexcept;
	std::wstring const& SessionName() const noexcept;
	std::wstring const& LogFileName() const noexcept;
	bool IsDone() const noexcept;

	static GUID const* GetProviderGuid(std::wstring const& name) noexcept;

	bool AddKernelEventTypes(std::initializer_list<KernelEventTypes> types);
	bool SetKernelEventTypes(std::initializer_list<KernelEventTypes> types);
	bool SetKernelEventTypes(std::span<KernelEventTypes const> types);
	std::vector<KernelEventTypes> GetKernelEventTypes() const noexcept;
	bool SetKernelEventStacks(std::initializer_list<std::wstring> categories);
	bool SetKernelEventStacks(std::span<std::wstring const> categories);
	std::vector<std::wstring> GetKernelEventStacks() const;
	// true if stacks were requested for any kernel category or provider
	bool HasStacks() const noexcept;
	bool AddProvider(GUID const& guid, int level = TRACE_LEVEL_INFORMATION, bool stacks = false);
	// adds the provider, or updates its configuration (re-enabling it if the session is live)
	bool AddProvider(GUID const& guid, ProviderConfig const& config);
	bool RemoveProvider(GUID const& guid);
	bool AddProvider(std::wstring const& name, int level = TRACE_LEVEL_INFORMATION);
	// restricts the provider's events to the given IDs (empty: all events)
	bool AddEventsForProvider(GUID const& guid, std::span<USHORT const> ids);
	std::vector<USHORT> GetEventsForProvider(GUID const& guid) const;
	void SetLogFileName(PCWSTR path);
	// real-time sessions: also log to this .etl file (set before Init)
	void SetOutputFile(std::wstring path);
	std::wstring const& OutputFile() const noexcept;
	// applied by Init
	void SetBufferSettings(BufferSettings const& settings) noexcept;
	BufferSettings const& GetBufferSettings() const noexcept;
	// true if the session was started with kernel events (system logger mode)
	bool IsKernelSession() const noexcept;
	// true if a trace session with this name is running (e.g. started by another tool)
	static bool SessionExists(PCWSTR name);

	std::vector<std::pair<GUID, ProviderConfig>> GetProviders() const noexcept;

	void Pause(bool pause) noexcept;
	bool IsRealTimeSession() const noexcept;
	bool Init();
	bool Start(EventCallback callback, bool cont = false);
	bool Start(EventCallbackNoOwn callback, bool cont = false);
	bool Stop(bool quit = false) noexcept;
	bool IsRunning() const noexcept;

	void ResetIndex(uint32_t index = 0);
	int UpdateEventConfig();

	static std::wstring GetProcessImageById(DWORD pid);
	static std::wstring GetDosNameFromNtName(PCWSTR name);

private:
	bool StartCommon(bool cont);
	ULONG EnableProvider(GUID const& guid, ProviderConfig const& config);
	static std::wstring GetProcessFullPath(DWORD pid);
	static void AddProcessName(DWORD pid, std::wstring name);
	static bool RemoveProcessName(DWORD pid);
	static void EnumProcesses();
	bool ParseProcessStartStop(EventData* data);
	void OnEventRecord(PEVENT_RECORD rec);
	DWORD Run();
	void HandleNoProcessId(EventData* data);
	void AttachKernelStack(PEVENT_RECORD rec);
	void AttachUserStack(PEVENT_RECORD rec);

private:
	static bool EnumProviders();

	struct ProcessInfo {
		DWORD Id;
		std::wstring ImageName;
		std::wstring FullPath;
	};

	std::vector<std::unique_ptr<EventData>> m_Events;
	// guards m_Events and m_RecentByThread, and is held while invoking the callback
	std::mutex m_EventsLock;
	// recent stored events per thread, used to attach kernel StackWalk events to their owners
	struct StackOwner {
		EventData* Event;
		ULONG64 StackTimeStamp{ 0 };	// raw EventTimeStamp of the StackWalk event(s) attached
	};
	std::unordered_map<DWORD, std::deque<StackOwner>> m_RecentByThread;
	// stored events whose user-mode stack is still to come, by MatchId (oldest dropped beyond a limit)
	std::unordered_map<ULONG64, EventData*> m_PendingUserStacks;
	std::deque<ULONG64> m_PendingUserStackOrder;
	// raw StackWalk EventTimeStamp -> converted event time: raw * 10^7 / Frequency + Offset (Offset learned at runtime)
	int64_t RawToEventTime(ULONG64 raw) const noexcept;
	ULONG64 m_StackTimeFrequency{ 10000000 };
	int64_t m_StackTimeOffset{ 0 };
	int m_StackTimeSamples{ 0 };
	bool m_TrackStacks{ false };
	TRACEHANDLE m_hTrace{ 0 };
	TRACEHANDLE m_hOpenTrace{ INVALID_PROCESSTRACE_HANDLE };
	EVENT_TRACE_PROPERTIES* m_Properties{};
	std::unique_ptr<BYTE[]> m_PropertiesBuffer;
	EVENT_TRACE_LOGFILE m_TraceLog { nullptr };
	wil::unique_handle m_hProcessThread;
	EventCallback m_Callback{};
	EventCallbackNoOwn m_CallbackNoOwn{};
	std::unordered_set<KernelEventTypes> m_KernelEventTypes;
	std::unordered_set<std::wstring> m_KernelEventStacks;
	inline static std::shared_mutex s_ProcessesLock;
	inline static std::unordered_map<DWORD, ProcessInfo> s_Processes;
	mutable std::unordered_map<ULONGLONG, std::wstring> m_KernelEventNames;
	std::vector<DWORD> m_CleanupPids;
	// log file position (timestamp + records seen at that timestamp), so a paused file resumes where it stopped
	struct FilePosition {
		LONGLONG Time{ 0 };
		uint32_t Count{ 0 };
	};
	FilePosition m_FilePos, m_ResumePos;
	wil::unique_threadpool_timer m_ProcessesTimer;
	std::shared_ptr<EventData> m_LastExcluded;
	uint32_t m_Index{ 0 };
	wil::unique_handle m_hMemMap;
	bool m_DumpUnnamedEvents{ false };
	inline static std::unordered_map<std::wstring, GUID> s_Providers;
	std::unordered_map<GUID, std::unordered_set<USHORT>> m_EventIds;
	std::unordered_map<GUID, ProviderConfig> m_Providers;
	std::wstring m_LogFileName;
	std::wstring m_OutputFile;
	BufferSettings m_Buffers;
	bool m_KernelSession{ false };
	std::wstring m_SessionName;
	FilterManager m_FilterMgr;
	std::mutex m_FilterLock;
	TRACE_LOGFILE_HEADER m_LogFileHeader;
	std::atomic<bool> m_Paused{ false };
	bool m_Continue{ false };
	bool m_Quit{ false };
	std::atomic<bool> m_Done{ false };
	std::atomic<bool> m_Stopped{ false };
	bool m_StoreEvents{ true };
	bool m_RealTimeSession{ false };
};


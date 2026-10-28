#pragma once

#include "KernelEvents.h"
#include <FilterManager.h>
#include <deque>
#include <optional>

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

// an image (exe, DLL, driver) loaded in a process, from the trace's image events
struct ImageInfo {
	ULONG64 Base;
	ULONG Size;
	ULONG TimeDateStamp;	// of the PE header; 0 if the event didn't have it
	std::wstring FileName;	// as in the event (an NT path)
	LONGLONG LoadTime;		// LLONG_MIN: loaded before the trace started
	LONGLONG UnloadTime{ LLONG_MAX };
};

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
	// safe to call while the session is running. Excluded events aren't handed to the callback; a session that
	// stores its events (EventCallbackNoOwn) still stores them, so a looser filter can show them later,
	// unless it discards them (SetDiscardExcluded)
	void SetFilterManager(FilterManager mgr);
	// a session that stores its events: don't store the ones the filter excludes (saves memory; a looser filter
	// can't bring them back). Safe to call while the session is running
	void SetDiscardExcluded(bool discard) noexcept;
	bool DiscardsExcluded() const noexcept;
	// deletes these stored events (e.g. the ones a filter excludes, when discarding them); the caller must not
	// hold other pointers to them. Returns how many were deleted
	size_t RemoveEvents(std::span<EventData* const> events);
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
	// what ETW makes of buffer settings when a session starts: at least 2 buffers per processor, and the
	// maximum not below the minimum (0: ETW's default, unchanged)
	static BufferSettings EffectiveBuffers(BufferSettings const& settings) noexcept;
	// the error of the last provider change on a running session that failed (AddProvider, RemoveProvider,
	// AddEventsForProvider), ERROR_SUCCESS if none did since the last call
	ULONG TakeLastControlError() noexcept;
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

	// the image holding the address in the process at that time, from the trace's image load/unload
	// and rundown events (drivers are in process 0); nullopt if the trace has none for it
	std::optional<ImageInfo> FindImage(DWORD pid, ULONG64 address, LONGLONG time) const;
	// an image load/unload/rundown event (the kernel's or Microsoft-Windows-Kernel-Process's)
	static bool IsImageEvent(EventData const& data) noexcept;
	// a live session: image events were selected (not only enabled to symbolize stacks)
	bool ImageEventsRequested() const noexcept {
		return !m_HideImageEvents;
	}

private:
	bool StartCommon(bool cont);
	ULONG EnableProvider(GUID const& guid, ProviderConfig const& config);
	static std::wstring GetProcessFullPath(DWORD pid);
	static void AddProcessName(DWORD pid, std::wstring name);
	static bool RemoveProcessName(DWORD pid);
	static void EnumProcesses();
	bool ParseProcessStartStop(EventData* data);
	// image load/unload/rundown events into m_Images; true if it was one
	bool ParseImageEvent(EventData* data);
	// with stacks, a non-kernel session also enables Microsoft-Windows-Kernel-Process's image events
	// (kernel sessions get the kernel's Image Load flag in UpdateEventConfig), so frames of processes
	// that exit can still be symbolized; image events nobody asked for aren't shown
	void UpdateImageTracking();
	void OnEventRecord(PEVENT_RECORD rec);
	DWORD Run();
	void CloseOpenTrace() noexcept;
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

	// process ID -> its images, in the order seen (written on the trace thread, read by the UI)
	std::unordered_map<DWORD, std::vector<ImageInfo>> m_Images;
	mutable std::shared_mutex m_ImagesLock;
	std::atomic<bool> m_HideImageEvents{ false };	// image events enabled only for symbols (not selected)
	bool m_ImageProviderAdded{ false };				// Kernel-Process enabled only for its image events

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
	// closed by whoever gets to it first: Stop (to cut ProcessTrace short) or Run (when it ends)
	std::atomic<TRACEHANDLE> m_hOpenTrace{ INVALID_PROCESSTRACE_HANDLE };
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
	std::atomic<bool> m_Quit{ false };
	std::atomic<bool> m_Done{ false };
	std::atomic<bool> m_Stopped{ false };
	bool m_StoreEvents{ true };
	bool m_RealTimeSession{ false };
	std::atomic<bool> m_DiscardExcluded{ false };
	ULONG m_LastControlError{ ERROR_SUCCESS };
};


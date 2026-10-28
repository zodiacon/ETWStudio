#pragma once

#include <map>
#include <optional>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <TraceSession.h>

//
// Resolves stack addresses to module!symbol+offset using DbgHelp.
// DbgHelp is single-threaded, so all of it runs on the symbolizer's own thread: the first lookups in a module
// may download its binary and symbols, and the UI shouldn't wait for that. The UI gets symbols as they come
// (Request); an export, already on a worker thread, waits for them (GetSymbols).
//
class StackSymbolizer final {
public:
	static StackSymbolizer& Get();

	// a frame to resolve. Its image (from the trace) is found by the caller, as the session's image list is
	// thread safe: the symbolizer's thread uses the session only as a key, so it may be gone by then.
	struct Frame {
		ULONG64 Address;
		DWORD Pid;
		std::optional<ImageInfo> Image;
	};
	// the modules come from the session's image events when it has them for the process at that time
	// (so processes that have exited, log files, other boots), else from the live process or kernel
	static Frame MakeFrame(TraceSession const* session, DWORD pid, ULONG64 address, LONGLONG time);

	// on the symbolizer's thread, for each frame in order: its index and symbol: module!symbol+offset,
	// module+offset without symbols, empty when no module is known
	using SymbolCallback = std::function<void(size_t, std::wstring const&)>;
	// queues the frames' lookup; Cancel(owner) drops what's still to be done
	void Request(void const* owner, TraceSession const* session, std::vector<Frame> frames, SymbolCallback callback);
	void Cancel(void const* owner);
	// looks the frames up and waits for them (not on the UI thread)
	std::vector<std::wstring> GetSymbols(TraceSession const* session, std::vector<Frame> frames);

	// drops the symbol contexts made for the session's images (when the session goes away)
	void Forget(TraceSession const* session);
	// drops all symbol contexts, e.g. after the symbol path changed
	void Reset();
	// at exit: drops the queued lookups, lets the one under way finish (for a moment) and cleans up DbgHelp
	void Shutdown();
	// what the symbolizer's thread is busy with that may take a while (finding an image or loading its
	// symbols, possibly from the symbol server); empty otherwise. For a progress dialog to show
	std::wstring GetActivity() const;

private:
	StackSymbolizer();
	// work queued for the symbolizer's thread; owner: who may cancel it (nullptr: no one)
	void Post(void const* owner, std::function<void()> work);
	void Run();

	// the rest runs on the symbolizer's thread only
	// from the cache, else DoResolve's
	std::wstring Resolve(TraceSession const* session, Frame const& frame);
	std::wstring DoResolve(TraceSession const* session, Frame const& frame);
	void DoReset();
	HANDLE GetKernelContext();
	HANDLE GetProcessContext(DWORD pid);
	bool InitContext(HANDLE hContext, bool invadeProcess);
	static std::wstring DriverPathToDosPath(std::wstring path);
	static std::wstring Lookup(HANDLE hContext, ULONG64 address);
	void SetActivity(std::wstring text);
	static BOOL CALLBACK OnSymbolEvent(HANDLE hContext, ULONG action, ULONG64 data, ULONG64 context);

	// a process of a trace, with the images loaded into it as frames needed them
	struct TraceContext {
		HANDLE Handle;		// fake: DbgHelp only uses it as a key
		std::unordered_map<ULONG64, std::wstring> Loaded;	// base -> image key
		std::unordered_set<std::wstring> Failed;			// image keys whose file wasn't found
	};
	// the image's text from the trace's own image list; nullopt: no file for it
	std::optional<std::wstring> LookupTraceImage(TraceSession const* session, DWORD pid, ImageInfo const& image, ULONG64 address);
	std::wstring FindImageFile(HANDLE hContext, ImageInfo const& image);

	std::unordered_map<DWORD, HANDLE> m_Processes;
	std::unordered_set<DWORD> m_FailedProcesses;
	std::map<std::pair<TraceSession const*, DWORD>, TraceContext> m_TraceContexts;
	// the frames' symbols so far, until the symbol path changes (or, for frames without a trace image, the session
	// goes): an export looks up the same addresses again and again, and looking up in DbgHelp is slow.
	// Frames in a trace image: by the image (file name, timestamp, size) and offset, in any process or session
	using ImageCacheKey = std::tuple<std::wstring, ULONG, ULONG, ULONG64>;
	std::map<ImageCacheKey, std::wstring> m_ImageCache;
	// frames without one come from the live process or kernel (whose modules DbgHelp takes once anyway): session, process (0: kernel), address
	using CacheKey = std::tuple<TraceSession const*, DWORD, ULONG64>;
	std::map<CacheKey, std::wstring> m_Cache;
	ULONG64 m_NextTraceHandle{ 0x7452430000000000ULL };
	bool m_KernelInit{ false };

	struct Task {
		void const* Owner;
		std::function<void()> Work;
	};
	mutable std::mutex m_ActivityLock;
	std::wstring m_Activity;
	std::mutex m_Lock;
	std::condition_variable m_Ready;
	std::deque<Task> m_Tasks;
	void const* m_RunningOwner{ nullptr };
	std::atomic<bool> m_CancelRunning{ false };
	bool m_Quit{ false };
	DWORD m_ThreadId{ 0 };
	std::thread m_Thread;	// last: starts running when constructed
};

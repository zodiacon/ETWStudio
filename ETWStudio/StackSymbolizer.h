#pragma once

#include <map>
#include <optional>

class TraceSession;
struct ImageInfo;

//
// Resolves stack addresses to module!symbol+offset using DbgHelp.
// DbgHelp is single-threaded: use from the UI thread only.
//
class StackSymbolizer final {
public:
	static StackSymbolizer& Get();
	~StackSymbolizer();

	// module!symbol+offset, module+offset without symbols, empty when no module is known.
	// The modules come from the session's image events when it has them for the process at that time
	// (so processes that have exited, log files, other boots), else from the live process or kernel.
	std::wstring GetSymbol(TraceSession const* session, DWORD pid, ULONG64 address, LONGLONG time);
	// drops the symbol contexts made for the session's images (before the session goes away)
	void Forget(TraceSession const* session);
	// drops all symbol contexts, e.g. after the symbol path changed
	void Reset();

private:
	StackSymbolizer() = default;
	HANDLE GetKernelContext();
	HANDLE GetProcessContext(DWORD pid);
	bool InitContext(HANDLE hContext, bool invadeProcess);
	static std::wstring DriverPathToDosPath(std::wstring path);
	static std::wstring Lookup(HANDLE hContext, ULONG64 address);

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
	ULONG64 m_NextTraceHandle{ 0x7452430000000000ULL };
	bool m_KernelInit{ false };
};

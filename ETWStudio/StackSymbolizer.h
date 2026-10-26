#pragma once

//
// Resolves stack addresses to module!symbol+offset using DbgHelp.
// DbgHelp is single-threaded: use from the UI thread only.
//
class StackSymbolizer final {
public:
	static StackSymbolizer& Get();
	~StackSymbolizer();

	std::wstring GetSymbol(DWORD pid, ULONG64 address);

private:
	StackSymbolizer() = default;
	HANDLE GetKernelContext();
	HANDLE GetProcessContext(DWORD pid);
	bool InitContext(HANDLE hContext, bool invadeProcess);
	static std::wstring DriverPathToDosPath(std::wstring path);

	std::unordered_map<DWORD, HANDLE> m_Processes;
	std::unordered_set<DWORD> m_FailedProcesses;
	bool m_KernelInit{ false };
};

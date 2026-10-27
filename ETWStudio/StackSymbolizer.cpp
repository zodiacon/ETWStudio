#include "pch.h"
#include "StackSymbolizer.h"
#include "AppSettings.h"
#include <DbgHelp.h>
#include <Psapi.h>

#pragma comment(lib, "dbghelp")

namespace {
	// fake DbgHelp context for kernel modules (not a real process handle)
	const HANDLE KernelContext = reinterpret_cast<HANDLE>(0x4B524E4CULL);

	bool IsKernelAddress(ULONG64 address) {
		return address >= 0xFFFF800000000000ULL;
	}

	// SizeOfImage from the file's PE header: with deferred loads, a module loaded without a size
	// covers no addresses, so nothing in it was ever found
	DWORD GetImageSize(std::wstring const& path) {
		wil::unique_hfile hFile(::CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr));
		if (!hFile)
			return 0;
		BYTE buffer[4096];
		DWORD read = 0;
		if (!::ReadFile(hFile.get(), buffer, sizeof(buffer), &read, nullptr))
			return 0;
		auto dos = reinterpret_cast<IMAGE_DOS_HEADER const*>(buffer);
		if (read < sizeof(*dos) || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > read)
			return 0;
		auto nt = reinterpret_cast<IMAGE_NT_HEADERS64 const*>(buffer + dos->e_lfanew);
		return nt->Signature == IMAGE_NT_SIGNATURE ? nt->OptionalHeader.SizeOfImage : 0;
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
		::SymLoadModuleExW(KernelContext, nullptr, dosPath.c_str(), nullptr, (DWORD64)base, GetImageSize(dosPath), nullptr, 0);
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

std::wstring StackSymbolizer::GetSymbol(DWORD pid, ULONG64 address) {
	auto hContext = IsKernelAddress(address) ? GetKernelContext() : GetProcessContext(pid);
	if (hContext) {
		IMAGEHLP_MODULEW64 module{ sizeof(module) };
		if (::SymGetModuleInfoW64(hContext, address, &module)) {
			BYTE buffer[sizeof(SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(WCHAR)];
			auto symbol = reinterpret_cast<PSYMBOL_INFOW>(buffer);
			symbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
			symbol->MaxNameLen = MAX_SYM_NAME;
			DWORD64 displacement = 0;
			if (::SymFromAddrW(hContext, address, &displacement, symbol))
				return std::format(L"{}!{}+0x{:X}", module.ModuleName, symbol->Name, displacement);
			return std::format(L"{}+0x{:X}", module.ModuleName, address - module.BaseOfImage);
		}
	}
	return L"";
}

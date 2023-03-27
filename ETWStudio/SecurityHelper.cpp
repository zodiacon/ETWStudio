#include "pch.h"
#include "SecurityHelper.h"

bool SecurityHelper::EnablePrivilege(PCWSTR privilege) {
	HANDLE hToken;
	if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
		return false;

	TOKEN_PRIVILEGES tp;
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	if (!::LookupPrivilegeValue(nullptr, privilege, &tp.Privileges[0].Luid))
		return false;

	BOOL success = ::AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
	::CloseHandle(hToken);

	return success && ::GetLastError() == ERROR_SUCCESS;
}


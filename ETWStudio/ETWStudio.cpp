// ETWStudio.cpp : main source file for ETWStudio.exe
//

#include "pch.h"
#include "MainFrm.h"
#include <WTLHelper.h>
#include "SecurityHelper.h"
#include "AppSettings.h"
#include "StackSymbolizer.h"
#include <DbgHelp.h>

#pragma comment(lib, "Dbghelp.lib")

CAppModule _Module;
AppSettings g_Settings;

// files on the command line ("Open with", or dropped on the exe), relative ones from the current directory
std::vector<std::wstring> CommandLineFiles(bool& newInstance) {
	std::vector<std::wstring> paths;
	newInstance = false;
	int argc = 0;
	if (auto argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc); argv) {
		for (int i = 1; i < argc; i++) {
			if (::_wcsicmp(argv[i], CMainFrame::NewInstanceSwitch) == 0) {
				newInstance = true;
				continue;
			}
			WCHAR full[MAX_PATH * 4];
			auto length = ::GetFullPathName(argv[i], _countof(full), full, nullptr);
			paths.push_back(length && length < _countof(full) ? full : argv[i]);
		}
		::LocalFree(argv);
	}
	return paths;
}

// startLock: held while starting (Single Instance), released once the window exists to be found
int Run(std::vector<std::wstring> const& paths, HANDLE startLock, int nCmdShow = SW_SHOWDEFAULT) {
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainFrame wndMain;

	auto created = wndMain.CreateEx() != nullptr;
	if (startLock)
		::ReleaseMutex(startLock);
	if (!created) {
		ATLTRACE(_T("Main window creation failed!\n"));
		return 0;
	}

	wndMain.ShowWindow(nCmdShow);

	if (!paths.empty()) {
		wndMain.UpdateWindow();		// shown before a session file's dialog
		wndMain.OpenFiles(paths);
	}

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow) {
	HRESULT hRes = ::CoInitialize(nullptr);
	ATLASSERT(SUCCEEDED(hRes));

	SecurityHelper::EnablePrivilege(SE_DEBUG_NAME);
	SecurityHelper::EnablePrivilege(SE_SYSTEM_PROFILE_NAME);

	::SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_CASE_INSENSITIVE | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES
#ifdef _DEBUG
		| SYMOPT_DEBUG
#endif
	);

	AtlInitCommonControls(ICC_BAR_CLASSES | ICC_LINK_CLASS | ICC_LISTVIEW_CLASSES);
	auto exist = AppSettings::Get().LoadFromKey(L"SOFTWARE\\ScorpioSoftware\\ETWStudio");
	AppSettings::Get().Upgrade();

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));
	if (exist)
		WTLHelper::InitDarkMode(AppSettings::Get().DarkMode() ? DarkModeKind::Dark : DarkModeKind::Light);
	else
		WTLHelper::InitDarkMode();

	// Single Instance: the files go to the running ETW Studio, if any. Instances starting together (several files
	// opened from Explorer at once) take turns, so the first one's window gets the others' files
	bool newInstance;
	auto paths = CommandLineFiles(newInstance);
	wil::unique_mutex_nothrow startLock;
	if (!newInstance && AppSettings::Get().SingleInstance()) {
		startLock.reset(::CreateMutex(nullptr, FALSE, L"Local\\ETWStudioStarting"));
		// (not available: one started elevated created it; go on without taking turns)
		if (startLock) {
			auto wait = ::WaitForSingleObject(startLock.get(), 10000);
			if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED)
				startLock.reset();
		}
		if (CMainFrame::PassToRunning(paths)) {
			if (startLock)
				::ReleaseMutex(startLock.get());
			_Module.Term();
			::CoUninitialize();
			return 0;
		}
	}

	int nRet = Run(paths, startLock.get(), nCmdShow);

	StackSymbolizer::Get().Shutdown();
	g_Settings.Save();

	_Module.Term();
	::CoUninitialize();

	return nRet;
}

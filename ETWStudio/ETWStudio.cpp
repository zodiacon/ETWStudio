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

int Run(LPCTSTR /*lpstrCmdLine*/ = nullptr, int nCmdShow = SW_SHOWDEFAULT) {
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainFrame wndMain;

	if (wndMain.CreateEx() == nullptr) {
		ATLTRACE(_T("Main window creation failed!\n"));
		return 0;
	}

	wndMain.ShowWindow(nCmdShow);

	// files on the command line ("Open with", or dropped on the exe), relative ones from the current directory
	int argc = 0;
	if (auto argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc); argv) {
		std::vector<std::wstring> paths;
		for (int i = 1; i < argc; i++) {
			WCHAR full[MAX_PATH * 4];
			auto length = ::GetFullPathName(argv[i], _countof(full), full, nullptr);
			paths.push_back(length && length < _countof(full) ? full : argv[i]);
		}
		::LocalFree(argv);
		if (!paths.empty()) {
			wndMain.UpdateWindow();		// shown before a session file's dialog
			wndMain.OpenFiles(paths);
		}
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

	int nRet = Run(lpstrCmdLine, nCmdShow);

	StackSymbolizer::Get().Shutdown();
	g_Settings.Save();

	_Module.Term();
	::CoUninitialize();

	return nRet;
}

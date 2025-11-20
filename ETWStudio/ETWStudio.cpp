// ETWStudio.cpp : main source file for ETWStudio.exe
//

#include "pch.h"
#include "MainFrm.h"
#include <ThemeHelper.h>
#include "SecurityHelper.h"
#include "AppSettings.h"
#include <DbgHelp.h>

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

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow) {
	HRESULT hRes = ::CoInitialize(NULL);
	ATLASSERT(SUCCEEDED(hRes));
	
	SecurityHelper::EnablePrivilege(SE_DEBUG_NAME);
	SecurityHelper::EnablePrivilege(SE_SYSTEM_PROFILE_NAME);

	::SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
	::LoadLibrary(L"SymSrv.dll");

	AtlInitCommonControls(ICC_BAR_CLASSES | ICC_LINK_CLASS | ICC_LISTVIEW_CLASSES);
	AppSettings::Get().LoadFromKey(L"SOFTWARE\\ScorpioSoftware\\ETWStudio");

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));
	ThemeHelper::Init();

	int nRet = Run(lpstrCmdLine, nCmdShow);

	g_Settings.Save();

	_Module.Term();
	::CoUninitialize();

	return nRet;
}

#include "pch.h"
#include <atlmisc.h>
#include "resource.h"
#include "AboutDlg.h"
#include "ProvidersView.h"
#include "MainFrm.h"
#include "StackSymbolizer.h"
#include "SimpleDlg.h"
#include "SymbolSettingsDlg.h"
#include "ProgressDlg.h"
#include "SessionDlg.h"
#include <ToolbarHelper.h>
#include <TraceSession.h>
#include "LogView.h"
#include "FullFindDlg.h"
#include "AppSettings.h"
#include "TraceSessionsView.h"
#include <WTLHelper.h>
#include <IconHelper.h>
#include <SecurityHelper.h>

const int WindowMenuPosition = 5;

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) {
	if (CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
		return TRUE;

	return m_view.PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle() {
	UIUpdateToolBar();
	return FALSE;
}

void CMainFrame::InitMenu(HMENU hMenu) {
	MenuItemData cmds[] = {
		{ ID_EDIT_COPY, IDI_COPY },
		{ ID_EDIT_FIND, IDI_FIND },
		{ ID_FILE_SAVE, IDI_SAVE },
		{ ID_FILE_OPEN, IDI_OPEN },
		{ ID_SESSION_RUN, IDI_RUN },
		{ ID_SESSION_STOP, IDI_STOP },
		{ ID_VIEW_AUTOSCROLL, IDI_AUTOSCROLL },
		{ ID_EDIT_CLEAR_ALL, IDI_CLEAR },
		{ ID_NEW_SESSION, IDI_SESSION },
		{ ID_EDIT_FILTER, IDI_FILTER },
		{ ID_EDIT_HIGHLIGHT, IDI_HIGHLIGHT },
		{ ID_VIEW_PROPERTIES, IDI_PROPERTIES },
		{ ID_FILE_RUNASADMINISTRATOR, 0, IconHelper::GetShieldIcon() },
	};
	WTLHelper::InitMenu(hMenu, cmds, _countof(cmds));

}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	// .etl and session files dropped anywhere on the window (the views are its children). Running elevated,
	// Windows drops Explorer's (not elevated) messages unless they're let through
	DragAcceptFiles();
	for (UINT msg : { (UINT)WM_DROPFILES, (UINT)WM_COPYDATA, 0x0049u /* WM_COPYGLOBALDATA */ })
		::ChangeWindowMessageFilterEx(m_hWnd, msg, MSGFLT_ALLOW, nullptr);

	CreateSimpleStatusBar();
	m_StatusBar.SubclassWindow(m_hWndStatusBar);
	// 0: ready, 1: run/stop icon, 2: event count, 3: messages (search results, exports), to the end
	int panes[] = { 100, 124, 350, -1 };
	m_StatusBar.SetParts(_countof(panes), panes);

	ToolBarButtonInfo const buttons[] = {
		{ ID_FILE_OPEN, IDI_OPEN },
		{ 0 },
		{ ID_EDIT_FIND, IDI_FIND },
		{ 0 },
		{ ID_NEW_SESSION, IDI_SESSION },
		{ ID_SESSION_RUN, IDI_RUN },
		{ ID_SESSION_STOP, IDI_STOP },
		{ 0 },
		{ ID_VIEW_AUTOSCROLL, IDI_AUTOSCROLL },
		{ ID_EDIT_CLEAR_ALL, IDI_CLEAR },
		{ 0 },
		{ ID_EDIT_FILTER, IDI_FILTER },
		{ ID_EDIT_HIGHLIGHT, IDI_HIGHLIGHT },
		{ 0 },
		{ ID_VIEW_PROPERTIES, IDI_PROPERTIES },
	};
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	auto tb = ToolbarHelper::CreateAndInitToolBar(m_hWnd, buttons, _countof(buttons));
	AddSimpleReBarBand(tb);
	UIAddToolBar(tb);

	m_view.m_bTabCloseButton = false;
	m_view.m_cchTabTextLength = 45;

	m_hWndClient = m_view.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);

	auto pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	CMenuHandle menuMain = GetMenu();
	if (SecurityHelper::IsRunningElevated()) {
		menuMain.GetSubMenu(0).DeleteMenu(0, MF_BYPOSITION);
		menuMain.GetSubMenu(0).DeleteMenu(0, MF_BYPOSITION);
	}
	m_view.SetWindowMenu(menuMain.GetSubMenu(WindowMenuPosition));
	// the File menu's submenus: the recent lists, told apart by their first command
	auto fileMenu = menuMain.GetSubMenu(0);
	for (int i = 0; i < fileMenu.GetMenuItemCount(); i++) {
		if (CMenuHandle sub = fileMenu.GetSubMenu(i); sub) {
			auto first = sub.GetMenuItemID(0);
			if (first == ID_FILE_RECENTFILE_FIRST)
				m_RecentFilesMenu = sub;
			else if (first == ID_FILE_RECENTSESSION_FIRST)
				m_RecentSessionsMenu = sub;
		}
	}
	UpdateRecentMenu(AppSettings::Recent::Files);
	UpdateRecentMenu(AppSettings::Recent::Sessions);

	CImageList images;
	images.Create(16, 16, ILC_COLOR32, 4, 4);
	UINT icons[] = {
		IDI_PROVIDERS, IDI_SESSION, IDI_PERF,
	};
	for (auto icon : icons)
		images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));

	m_view.SetImageList(images);

	auto lf = AppSettings::Get().Font();
	if (lf.lfFaceName[0])
		m_Font.CreateFontIndirect(&lf);

	InitMenu(GetMenu());
	UIAddMenu(GetMenu());
	UISetCheck(ID_VIEW_STATUS_BAR, AppSettings::Get().ViewStatusBar());
	UISetCheck(ID_OPTIONS_DARKMODE, WTLHelper::IsDarkMode());

	UpdateUI();

	return 0;
}

LRESULT CMainFrame::OnShowWindow(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	static bool shown = false;
	if (!shown) {
		shown = true;
		auto wp = AppSettings::Get().MainWindowPlacement();
		if (wp.showCmd)
			SetWindowPlacement(&wp);
		SetAlwaysOnTop(AppSettings::Get().AlwaysOnTop());
	}
	return 0;
}

void CMainFrame::SetStatusText(int pane, PCWSTR text) {
	m_StatusBar.SetText(pane, text);
}

void CMainFrame::SetStatusIcon(int pane, HICON hIcon) {
	m_StatusBar.SetIcon(pane, hIcon);
}

CUpdateUIBase& CMainFrame::UI() {
	return *this;
}

HFONT CMainFrame::GetFont() const {
	return m_Font.m_hFont;
}

void CMainFrame::ApplyFontToAllPages() {
	for (int i = 0; i < m_view.GetPageCount(); i++)
		::SendMessage(m_view.GetPageHWND(i), WM_UPDATE_FONT, (WPARAM)m_Font.m_hFont, 0);
}

LRESULT CMainFrame::OnChangeFont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	LOGFONT lf{};
	if (m_Font.m_hFont)
		m_Font.GetLogFont(&lf);
	else
		::GetObject(::GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);

	CFontDialog dlg(&lf, CF_SCREENFONTS | CF_FORCEFONTEXIST | CF_NOVERTFONTS, nullptr, m_hWnd);
	if(IDOK == dlg.DoModal()) {
		dlg.GetCurrentFont(&lf);
		if (m_Font.m_hFont)
			m_Font.DeleteObject();
		m_Font.CreateFontIndirect(&lf);
		AppSettings::Get().Font(lf);
		ApplyFontToAllPages();
	}
	return 0;
}

LRESULT CMainFrame::OnResetFont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_Font.m_hFont)
		m_Font.DeleteObject();
	AppSettings::Get().Font(LOGFONT{});
	ApplyFontToAllPages();
	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	WINDOWPLACEMENT wp{ sizeof(wp) };
	GetWindowPlacement(&wp);
	AppSettings::Get().MainWindowPlacement(wp);

	auto pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	bHandled = FALSE;
	return 1;
}

void CMainFrame::SetAlwaysOnTop(bool onTop) {
	SetWindowPos(onTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	AppSettings::Get().AlwaysOnTop(onTop);
	UISetCheck(ID_OPTIONS_ALWAYSONTOP, onTop);
}

LRESULT CMainFrame::OnAlwaysOnTop(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	bool top = (GetExStyle() & WS_EX_TOPMOST) == 0;
	SetAlwaysOnTop(top);
	return 0;
}

LRESULT CMainFrame::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	m_view.RemoveAllPages();
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT CMainFrame::OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	// an export or a search under way: stop it first, then close (the frame is disabled, but a posted
	// WM_CLOSE still gets here, e.g. at log off)
	if (auto progress = CProgressDlg::Running()) {
		progress->CancelAndClose(m_hWnd);
		return 0;
	}
	bHandled = FALSE;
	return 0;
}

LRESULT CMainFrame::OnViewProviders(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto pView = new CProvidersView(this);
	pView->Create(m_view, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
	m_view.AddPage(pView->m_hWnd, L"Registered Providers", 0, pView);

	return 0;
}

LRESULT CMainFrame::OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto bVisible = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}

LRESULT CMainFrame::OnWindowClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int nActivePage = m_view.GetActivePage();
	if (nActivePage != -1)
		m_view.RemovePage(nActivePage);
	else
		::MessageBeep((UINT)-1);

	if (m_view.GetPageCount() == 0)
		UpdateUI();

	return 0;
}

LRESULT CMainFrame::OnWindowCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	m_view.RemoveAllPages();
	UpdateUI();

	return 0;
}

LRESULT CMainFrame::OnWindowActivate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int nPage = wID - ID_WINDOW_TABFIRST;
	if(nPage < m_view.GetPageCount())
		m_view.SetActivePage(nPage);

	return 0;
}

LRESULT CMainFrame::OnNewSession(WORD, WORD, HWND, BOOL&) {
	NewSession(nullptr);
	return 0;
}

LRESULT CMainFrame::OnLoadSession(WORD, WORD, HWND, BOOL&) {
	CSimpleFileDialog dlg(TRUE, L"etwsession", nullptr, OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_ENABLESIZING,
		L"ETW Studio Sessions (*.etwsession)\0*.etwsession\0All Files\0*.*\0", m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = IDOK == dlg.DoModal();
	WTLHelper::ResumeHook();
	if (!ok)
		return 0;

	if (!LoadSession(dlg.m_szFileName))
		AtlMessageBox(m_hWnd, L"Failed to load the session file", IDS_TITLE, MB_ICONERROR);
	return 0;
}

bool CMainFrame::LoadSession(PCWSTR path) {
	auto preset = SessionPreset::Load(path);
	if (!preset)
		return false;
	AddRecentSession(path);
	NewSession(&*preset);
	return true;
}

LRESULT CMainFrame::OnRecentSession(WORD, WORD id, HWND, BOOL&) {
	auto paths = AppSettings::Get().RecentList(AppSettings::Recent::Sessions);
	auto index = id - ID_FILE_RECENTSESSION_FIRST;
	if (index >= paths.size())
		return 0;
	auto& path = paths[index];
	if (!LoadSession(path.c_str())) {
		auto text = std::format(L"Failed to load the session file {}.\n\nRemove it from the recent sessions?", path);
		if (IDYES == AtlMessageBox(m_hWnd, text.c_str(), IDS_TITLE, MB_ICONERROR | MB_YESNO)) {
			AppSettings::Get().UpdateRecent(AppSettings::Recent::Sessions, path, true);
			UpdateRecentMenu(AppSettings::Recent::Sessions);
		}
	}
	return 0;
}

LRESULT CMainFrame::OnRecentFile(WORD, WORD id, HWND, BOOL&) {
	auto paths = AppSettings::Get().RecentList(AppSettings::Recent::Files);
	auto index = id - ID_FILE_RECENTFILE_FIRST;
	if (index >= paths.size())
		return 0;
	auto& path = paths[index];
	if (!OpenFile(path.c_str())) {
		auto text = std::format(L"Failed to open {}.\n\nRemove it from the recent files?", path);
		if (IDYES == AtlMessageBox(m_hWnd, text.c_str(), IDS_TITLE, MB_ICONERROR | MB_YESNO)) {
			AppSettings::Get().UpdateRecent(AppSettings::Recent::Files, path, true);
			UpdateRecentMenu(AppSettings::Recent::Files);
		}
	}
	return 0;
}

LRESULT CMainFrame::OnClearRecent(WORD, WORD id, HWND, BOOL&) {
	auto kind = id == ID_FILE_CLEARRECENTFILES ? AppSettings::Recent::Files : AppSettings::Recent::Sessions;
	AppSettings::Get().ClearRecent(kind);
	UpdateRecentMenu(kind);
	return 0;
}

void CMainFrame::AddRecentSession(PCWSTR path) {
	AppSettings::Get().UpdateRecent(AppSettings::Recent::Sessions, path);
	UpdateRecentMenu(AppSettings::Recent::Sessions);
}

void CMainFrame::UpdateRecentMenu(AppSettings::Recent kind) {
	bool files = kind == AppSettings::Recent::Files;
	auto menu = files ? m_RecentFilesMenu : m_RecentSessionsMenu;
	UINT first = files ? ID_FILE_RECENTFILE_FIRST : ID_FILE_RECENTSESSION_FIRST;
	if (!menu)
		return;
	while (menu.GetMenuItemCount() > 0)
		menu.DeleteMenu(0, MF_BYPOSITION);

	auto paths = AppSettings::Get().RecentList(kind);
	if (paths.empty()) {
		menu.AppendMenu(MF_STRING | MF_GRAYED, first, L"(None)");
		return;
	}
	for (int i = 0; i < (int)paths.size(); i++) {
		WCHAR compact[64];
		if (!AtlCompactPath(compact, paths[i].c_str(), _countof(compact)))
			wcscpy_s(compact, paths[i].substr(0, _countof(compact) - 1).c_str());
		// & would mark a mnemonic
		std::wstring text(compact);
		for (size_t pos = 0; (pos = text.find(L'&', pos)) != std::wstring::npos; pos += 2)
			text.insert(pos, 1, L'&');
		menu.AppendMenu(MF_STRING, first + i, std::format(L"&{} {}", i + 1, text).c_str());
	}
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, files ? ID_FILE_CLEARRECENTFILES : ID_FILE_CLEARRECENTSESSIONS, L"&Clear List");
}

void CMainFrame::NewSession(SessionPreset const* preset) {
	auto name = std::format(L"ETWStudioSession{}", m_view.GetPageCount() + 1);
	auto session = std::make_unique<TraceSession>(name);
	CSessionDlg dlg(this, *session);
	if (preset)
		dlg.SetPreset(*preset);
	if (IDOK == dlg.DoModal()) {
		CWaitCursor wait;
		if (!session->Init()) {
			AtlMessageBox(m_hWnd, L"Failed to initialize trace session", IDS_TITLE, MB_ICONERROR);
			return;
		}
		// the dialog already configured the session's providers
		auto title = session->SessionName();
		auto view = new CLogView(this, std::move(session));
		view->Create(m_view, rcDefault, nullptr,
			WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
		m_view.AddPage(view->m_hWnd, title.c_str(), 1, view);
		PostMessage(WM_COMMAND, ID_SESSION_RUN);
	}
}

UINT CMainFrame::DisplayContextMenu(HMENU hMenu, int x, int y, DWORD flags) {
	InitMenu(hMenu);
	return (UINT)TrackPopupMenuEx(hMenu, flags, x, y, m_hWnd, nullptr);
}

LRESULT CMainFrame::OnToggleDarkMode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto& settings = AppSettings::Get();
	settings.DarkMode(!settings.DarkMode());
	WTLHelper::SwitchToMode(settings.DarkMode() ? DarkModeKind::Dark : DarkModeKind::Light, m_hWnd);
	InitMenu(GetMenu());
	DrawMenuBar();
	UISetCheck(ID_OPTIONS_DARKMODE, WTLHelper::IsDarkMode());

	return 0;
}

LRESULT CMainFrame::OnFindAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CFullFindDlg dlg;
	dlg.DoModal();

	return 0;
}

LRESULT CMainFrame::OnPageActivated(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/) {
	if (m_ActivePage >= 0 && m_ActivePage < m_view.GetPageCount())
		::SendMessage(m_view.GetPageHWND(m_ActivePage), WM_ACTIVATE, 0, 0);
	m_ActivePage = m_view.GetActivePage();
	if(m_ActivePage >= 0)
		::SendMessage(m_view.GetPageHWND(m_ActivePage), WM_ACTIVATE, 1, 0);

	return 0;
}

void CMainFrame::UpdateUI() {
	UIEnable(ID_EDIT_COPY, false);
	UIEnable(ID_SESSION_RUN, false);
	UIEnable(ID_SESSION_STOP, false);
	UIEnable(ID_VIEW_AUTOSCROLL, false);
	UIEnable(ID_EDIT_FIND, false);
	UIEnable(ID_EDIT_FIND_NEXT, false);
	UIEnable(ID_EDIT_FIND_PREVIOUS, false);
	UIEnable(ID_EDIT_CLEAR_ALL, false);
	UISetCheck(ID_SESSION_RUN, false);
	UISetCheck(ID_SESSION_STOP, false);
	UISetCheck(ID_VIEW_AUTOSCROLL, false);
	UIEnable(ID_VIEW_IMAGEEVENTS, false);
	UISetCheck(ID_VIEW_IMAGEEVENTS, false);
	UIEnable(ID_EDIT_HIGHLIGHT, false);
	UIEnable(ID_EDIT_FILTER, false);
	UIEnable(ID_FILE_SAVE, false);
	UIEnable(ID_VIEW_PROPERTIES, false);
	SetStatusText(2, L"");
	SetStatusIcon(1, nullptr);
}

LRESULT CMainFrame::OnAboutWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const {
	::TrySubmitThreadpoolCallback([](auto, auto ctx) {
		::ShellAbout(nullptr, L"ETW Studio", nullptr, nullptr);
		}, nullptr, nullptr);

	return 0;
}

LRESULT CMainFrame::OnViewTraceSessions(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto pView = new CTraceSessionsView(this);
	pView->Create(m_view, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
	m_view.AddPage(pView->m_hWnd, L"Sessions", 1, pView);
	return 0;
}

LRESULT CMainFrame::OnFileOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CSimpleFileDialog dlg(TRUE, L"etl", nullptr, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_ENABLESIZING,
		L"Event Trace Files (*.etl)\0*.Etl\0CSV Files (*csv)\0*.csv\0All Files\0*.*\0");
	WTLHelper::SuspendHook();
	bool ok = IDOK == dlg.DoModal();
	WTLHelper::ResumeHook();
	if (ok && !OpenFile(dlg.m_szFileName))
		AtlMessageBox(m_hWnd, L"Failed to open file", IDR_MAINFRAME, MB_ICONERROR);
	return 0;
}

LRESULT CMainFrame::OnDropFiles(UINT, WPARAM wParam, LPARAM, BOOL&) {
	auto hDrop = (HDROP)wParam;
	std::vector<std::wstring> paths;
	auto count = ::DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
	for (UINT i = 0; i < count; i++) {
		std::wstring path(::DragQueryFile(hDrop, i, nullptr, 0), L'\0');
		::DragQueryFile(hDrop, i, path.data(), (UINT)path.size() + 1);
		paths.push_back(std::move(path));
	}
	// done with the drop before a session file's dialog (modal) comes up
	::DragFinish(hDrop);
	OpenFiles(paths);
	return 0;
}

void CMainFrame::OpenFiles(std::vector<std::wstring> const& paths) {
	std::wstring failed;
	for (auto& path : paths) {
		auto dot = path.find_last_of(L'.');
		auto ext = dot == std::wstring::npos ? std::wstring() : path.substr(dot);
		if (::_wcsicmp(ext.c_str(), L".etl") == 0) {
			if (!OpenFile(path.c_str()))
				failed += path + L"\n";
		}
		else if (::_wcsicmp(ext.c_str(), L".etwsession") == 0) {
			if (!LoadSession(path.c_str()))
				failed += path + L"\n";
		}
		else
			failed += path + L" (only .etl and .etwsession files)\n";
	}
	if (!failed.empty())
		AtlMessageBox(m_hWnd, (L"Failed to open:\n\n" + failed).c_str(), IDS_TITLE, MB_ICONERROR);
}

bool CMainFrame::OpenFile(PCWSTR path) {
	auto view = new CLogView(this, nullptr);
	view->Create(m_view, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
	if (!view->OpenLogFile(path)) {
		view->DestroyWindow();
		return false;
	}
	auto slash = ::wcsrchr(path, L'\\');
	m_view.AddPage(view->m_hWnd, slash ? slash + 1 : path, 2, view);
	AppSettings::Get().UpdateRecent(AppSettings::Recent::Files, path);
	UpdateRecentMenu(AppSettings::Recent::Files);
	return true;
}

LRESULT CMainFrame::OnRunAsAdmin(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	// the elevated instance reopens the files open here (its command line), in tab order
	std::wstring files;
	for (int i = 0; i < m_view.GetPageCount(); i++)
		if (auto view = dynamic_cast<CLogView*>((CMessageMap*)m_view.GetPageData(i)); view && !view->OpenedFile().empty())
			files += L"\"" + view->OpenedFile() + L"\" ";
	if (SecurityHelper::RunElevated(files.empty() ? nullptr : files.c_str(), true)) {
		PostMessage(WM_CLOSE);
	}
	return 0;
}

LRESULT CMainFrame::OnSymbolPath(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CSymbolSettingsDlg dlg;
	if (IDOK == dlg.DoModal())
		StackSymbolizer::Get().Reset();		// stacks shown from now on use the new path
	return 0;
}

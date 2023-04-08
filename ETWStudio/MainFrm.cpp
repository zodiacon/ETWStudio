#include "pch.h"
#include "resource.h"
#include "AboutDlg.h"
#include "ProvidersView.h"
#include "MainFrm.h"
#include "SessionDlg.h"
#include <ToolbarHelper.h>
#include <TraceSession.h>
#include "LogView.h"
#include "FullFindDlg.h"

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

void CMainFrame::InitMenu() {
	struct {
		UINT id, icon;
		HICON hIcon = nullptr;
	} cmds[] = {
		{ ID_EDIT_COPY, IDI_COPY },
		{ ID_EDIT_CUT, IDI_CUT },
		{ ID_EDIT_PASTE, IDI_PASTE },
		{ ID_EDIT_FIND, IDI_FIND },
		{ ID_FILE_SAVE, IDI_SAVE },
		{ ID_FILE_OPEN, IDI_OPEN },
		{ ID_SESSION_RUN, IDI_RUN },
		{ ID_SESSION_STOP, IDI_STOP },
		{ ID_VIEW_AUTOSCROLL, IDI_AUTOSCROLL },
		//{ ID_EDIT_DELETE, IDI_CANCEL },
		//{ ID_EDIT_CLEAR_ALL, IDI_ERASE },
		{ ID_TRACING_REGISTEREDPROVIDERS, IDI_PROVIDERS },
		{ ID_NEW_SESSION, IDI_SESSION },
	};
	for (auto& cmd : cmds) {
		if (cmd.icon)
			AddCommand(cmd.id, cmd.icon);
		else
			AddCommand(cmd.id, cmd.hIcon);
	}
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	CreateSimpleStatusBar();

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
		//{ ID_EDIT_DELETE, IDI_CANCEL },
		//{ ID_EDIT_CLEAR_ALL, IDI_ERASE },
		{ 0 },
		{ ID_TRACING_REGISTEREDPROVIDERS, IDI_PROVIDERS },
	};
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	auto tb = ToolbarHelper::CreateAndInitToolBar(m_hWnd, buttons, _countof(buttons));
	AddSimpleReBarBand(tb);
	UIAddToolBar(tb);

	m_view.m_bTabCloseButton = false;
	m_hWndClient = m_view.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);

	auto pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	CMenuHandle menuMain = GetMenu();
	m_view.SetWindowMenu(menuMain.GetSubMenu(WindowMenuPosition));

	CImageList images;
	images.Create(16, 16, ILC_COLOR32, 4, 4);
	UINT icons[] = {
		IDI_PROVIDERS, IDI_SESSION,
	};
	for (auto icon : icons)
		images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));

	m_view.SetImageList(images);

	InitMenu();
	AddMenu(GetMenu());
	UIAddMenu(GetMenu());
	UpdateUI();

	return 0;
}

CUpdateUIBase& CMainFrame::UI() {
	return *this;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	bHandled = FALSE;
	return 1;
}

LRESULT CMainFrame::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	PostMessage(WM_CLOSE);
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
	m_view.SetActivePage(nPage);

	return 0;
}

LRESULT CMainFrame::OnNewSession(WORD, WORD, HWND, BOOL&) {
	auto name = std::format(L"LogSession{}", m_view.GetPageCount() + 1);
	auto session = std::make_unique<TraceSession>(name);
	CSessionDlg dlg(this, *session);
	if (IDOK == dlg.DoModal()) {
		if (session->GetProviders().empty()) {
			AtlMessageBox(m_hWnd, L"Session must have at least one provider", 
				IDS_TITLE, MB_ICONERROR);
			return 0;
		}
		CWaitCursor wait;
		auto view = new CLogView(this, std::move(session));
		view->Create(m_view, rcDefault, nullptr,
			WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
		m_view.AddPage(view->m_hWnd, name.c_str(), 1, view);
	}
	return 0;
}

UINT CMainFrame::DisplayContextMenu(HMENU hMenu, int x, int y, DWORD flags) {
	return ShowContextMenu(hMenu, flags, x, y);
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
	::SendMessage(m_view.GetPageHWND(m_ActivePage), WM_ACTIVATE, 1, 0);

	return 0;
}

void CMainFrame::UpdateUI() {
	UIEnable(ID_EDIT_COPY, false);
	UIEnable(ID_SESSION_RUN, false);
	UIEnable(ID_SESSION_STOP, false);
	UIEnable(ID_SESSION_CLEAR, false);
	UIEnable(ID_VIEW_AUTOSCROLL, false);
	UIEnable(ID_EDIT_FIND, false);
	UISetCheck(ID_SESSION_RUN, false);
	UISetCheck(ID_SESSION_STOP, false);
}

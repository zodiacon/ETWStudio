#include "pch.h"
#include "SessionDlg.h"
#include <ThemeHelper.h>
#include "ProvidersDlg.h"
#include "Interfaces.h"
#include "StringHelper.h"
#include "SimpleDlg.h"

CSessionDlg::CSessionDlg(IMainFrame* frame, TraceSession& session) : m_pFrame(frame), m_Session(session) {
}

CString CSessionDlg::GetColumnText(HWND, int row, int col) const {
	auto& pi = m_Providers[row];
	switch (col) {
		case 0: return pi.Name.c_str();
		case 1: return StringHelper::GuidToString(pi.Guid).c_str();
		case 2: return StringHelper::LevelToString(pi.Level);
	}
	return L"";
}

LRESULT CSessionDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	SetDialogIcon(IDI_SESSION);

	SetDlgItemText(IDC_NAME, m_Session.SessionName().c_str());
	CheckDlgButton(IDC_REALTIME, BST_CHECKED);

	m_List.Attach(GetDlgItem(IDC_LIST));
	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Name", 0, 200);
	cm->AddColumn(L"GUID", 0, 200);
	cm->AddColumn(L"Level", 0, 60);

	return 0;
}

LRESULT CSessionDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	if (wID == IDOK) {
		CWaitCursor wait;
		m_Session.Init();
		for (auto& p : m_Providers)
			m_Session.AddProvider(p.Guid);
	}
	EndDialog(wID);
	return 0;
}

LRESULT CSessionDlg::OnBrowseFile(WORD, WORD wID, HWND, BOOL&) {
	CSimpleFileDialog dlg(FALSE, L"etl", L"log", OFN_ENABLESIZING | OFN_EXPLORER | OFN_OVERWRITEPROMPT,
		L"Event Tracing Log files (*.etl)\0*.etl*\0All Files\0*.*\0", m_hWnd);
	ThemeHelper::Suspend();
	auto ok = IDOK == dlg.DoModal();
	ThemeHelper::Resume();
	if (ok) {
		SetDlgItemText(IDC_PATH, dlg.m_szFileName);
	}
	return 0;
}

LRESULT CSessionDlg::OnProviderDropdown(int, LPNMHDR, BOOL&) {
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	CRect rc;
	GetDlgItem(IDC_ADD).GetWindowRect(&rc);
	auto cmd = m_pFrame->DisplayContextMenu(menu.GetSubMenu(0), rc.left, rc.bottom, TPM_RETURNCMD);
	if (cmd) {
		LRESULT result;
		ProcessWindowMessage(m_hWnd, WM_COMMAND, cmd, 0, result);
	}
	return 0;
}

LRESULT CSessionDlg::OnRegisteredProvider(WORD, WORD wID, HWND, BOOL&) {
	CProvidersDlg dlg;
	if (IDOK == dlg.DoModal()) {
		ProviderInfo pi;
		auto p = dlg.GetSelectedProvider();
		ATLASSERT(p);
		pi.Guid = p->Guid();
		pi.Name = p->Name();
		pi.Level = 0;
		m_Providers.push_back(std::move(pi));
		m_List.SetItemCountEx((int)m_Providers.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	}
	return 0;
}

LRESULT CSessionDlg::OnGuidProvider(WORD, WORD wID, HWND, BOOL&) {
	CSimpleDlg dlg;
	if (IDOK == dlg.DoModal()) {
		ProviderInfo pi;
		if (S_OK != ::CLSIDFromString(dlg.GetText(), &pi.Guid)) {
			AtlMessageBox(m_hWnd, L"Invalid GUID", IDS_TITLE, MB_ICONERROR);
			return 0;
		}
		pi.Level = 0;
		m_Providers.push_back(std::move(pi));
		m_List.SetItemCountEx((int)m_Providers.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	}
	return 0;
}

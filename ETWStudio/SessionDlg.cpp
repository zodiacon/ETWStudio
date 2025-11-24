#include "pch.h"
#include "SessionDlg.h"
#include <ThemeHelper.h>
#include "ProvidersDlg.h"
#include "Interfaces.h"
#include "StringHelper.h"
#include "SimpleDlg.h"
#include "KernelProviderDlg.h"

CSessionDlg::CSessionDlg(IMainFrame* frame, TraceSession& session, bool edit) : m_pFrame(frame), m_Session(session), m_Edit(edit) {
}

CString CSessionDlg::GetColumnText(HWND, int row, int col) const {
	auto& pi = m_Providers[row];
	switch (col) {
		case 0: return pi.Name.c_str();
		case 1: return StringHelper::GuidToString(pi.Guid).c_str();
		case 2: return StringHelper::LevelToString(pi.Level);
		case 3: return std::format(L"0x{:016X}", pi.MatchAllKeyword).c_str();
		case 4: return std::format(L"0x{:016X}", pi.MatchAnyKeyword).c_str();
	}
	return L"";
}

void CSessionDlg::OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState) const {
	GetDlgItem(IDC_REMOVE).EnableWindow(m_List.GetSelectedCount() == 1);
}

std::vector<CSessionDlg::ProviderInfo> const& CSessionDlg::GetProviders() const {
	return m_Providers;
}

LRESULT CSessionDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	SetDialogIcon(IDI_SESSION);

	SetDlgItemText(IDC_NAME, m_Session.SessionName().c_str());
	CheckDlgButton(IDC_REALTIME, BST_CHECKED);

	m_List.Attach(GetDlgItem(IDC_LIST));
	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Name", 0, 170);
	cm->AddColumn(L"GUID", 0, 170);
	cm->AddColumn(L"Level", 0, 60);
	cm->AddColumn(L"Match All", LVCFMT_RIGHT, 120);
	cm->AddColumn(L"Match Any", LVCFMT_RIGHT, 120);

	if (m_Edit) {
		for (auto& [guid, level] : m_Session.GetProviders()) {
			ProviderInfo pi;
			pi.Guid = guid;
			pi.Level = level;
			pi.Name = StringHelper::ProviderGuidToName(guid);
			m_Providers.push_back(std::move(pi));
		}
		m_List.SetItemCount((int)m_Providers.size());
		SetWindowText((L"Edit Session: " + m_Session.SessionName()).c_str());
	}
	return 0;
}

LRESULT CSessionDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	if (wID == IDOK) {
		CString name;
		GetDlgItemText(IDC_NAME, name);
		m_Session.SetSessionName((PCWSTR)name);
		if (m_Providers.empty()) {
			AtlMessageBox(m_hWnd, L"Session must have at least one provider",
				IDS_TITLE, MB_ICONWARNING);
			return 0;
		}
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
			AtlMessageBox(m_hWnd, L"Invalid GUID", IDS_TITLE, MB_ICONWARNING);
			return 0;
		}
		m_Providers.push_back(std::move(pi));
		m_List.SetItemCountEx((int)m_Providers.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	}
	return 0;
}

LRESULT CSessionDlg::OnKernelProvider(WORD, WORD wID, HWND, BOOL&) {
	CKernelProviderDlg dlg;
	if (IDOK == dlg.DoModal()) {
	}
	return 0;
}

LRESULT CSessionDlg::OnRemoveProvider(WORD, WORD wID, HWND, BOOL&) {
	int selected = m_List.GetSelectedIndex();
	ATLASSERT(selected >= 0);
	m_Providers.erase(m_Providers.begin() + selected);
	m_List.SetItemCountEx((int)m_Providers.size(), LVSICF_NOSCROLL);

	return 0;
}

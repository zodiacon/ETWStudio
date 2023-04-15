#include "pch.h"
#include "resource.h"
#include "FilterDlg.h"

CFilterDlg::CFilterDlg(FilterManager& fm) : m_FilterMgr(fm) {
}

LRESULT CFilterDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	((CButton)GetDlgItem(IDC_UP)).SetIcon(AtlLoadIconImage(IDI_UP, 0, 16, 16));
	((CButton)GetDlgItem(IDC_DOWN)).SetIcon(AtlLoadIconImage(IDI_DOWN, 0, 16, 16));

	m_List.Attach(GetDlgItem(IDC_LIST));
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_CHECKBOXES);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Name", 0, 100);
	cm->AddColumn(L"Compare Type", 0, 60);
	cm->AddColumn(L"Value", 0, 140);
	cm->AddColumn(L"Action", 0, 60);

	return 0;
}

LRESULT CFilterDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	EndDialog(wID);
	return 0;
}

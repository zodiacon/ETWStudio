#include "pch.h"
#include "resource.h"
#include "FilterDlg.h"

CFilterDlg::CFilterDlg(FilterManager& fm) : m_FilterMgr(fm) {
}

LRESULT CFilterDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	((CButton)GetDlgItem(IDC_UP)).SetIcon(AtlLoadIconImage(IDI_UP, 0, 24, 24));
	((CButton)GetDlgItem(IDC_DOWN)).SetIcon(AtlLoadIconImage(IDI_DOWN, 0, 24, 24));
	((CButton)GetDlgItem(IDC_SAVE)).SetIcon(AtlLoadIconImage(IDI_SAVE, 0, 24, 24));
	((CButton)GetDlgItem(IDC_LOAD)).SetIcon(AtlLoadIconImage(IDI_OPEN, 0, 24, 24));
	((CButton)GetDlgItem(IDC_CLEAR)).SetIcon(AtlLoadIconImage(IDI_CLEAR, 0, 24, 24));
	((CButton)GetDlgItem(IDC_ADD)).SetIcon(AtlLoadIconImage(IDI_FILTER_ADD, 0, 24, 24));

	SetDialogIcon(IDI_FILTER);

	m_List.Attach(GetDlgItem(IDC_LIST));
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_CHECKBOXES);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Name", 0, 100);
	cm->AddColumn(L"Compare Type", 0, 60);
	cm->AddColumn(L"Value", 0, 140);
	cm->AddColumn(L"Action", 0, 60);

	static PCWSTR standardProperties[] = {
		L"Process ID",
		L"Thread ID",
		L"Index",
		L"Level",
		L"Channel",
		L"Task",
		L"Keyword",
		L"Event Name",
		L"Version",
		L"Opcode",
	};
	m_PropCB.Attach(GetDlgItem(IDC_PROPERTY));
	for (auto& prop : standardProperties)
		m_PropCB.AddString(prop);

	static PCWSTR compareTypes[] = {
		L"Equal",
		L"Not Equal",
		L"Less Than",
		L"Greater Than",
		L"Less Than Or Equal",
		L"Greater Than Or Equal",
		L"Contains",
		L"Does Not Contain",
		L"Starts With",
		L"Ends With",
		//L"In Range",
		//L"Not In Range",
	};
	m_CompCB.Attach(GetDlgItem(IDC_COMPARE));
	int i = 0;
	for (auto& comp : compareTypes) {
		int n = m_CompCB.AddString(comp);
		m_CompCB.SetItemData(n, i++);
	}
	return 0;
}

LRESULT CFilterDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	EndDialog(wID);
	return 0;
}

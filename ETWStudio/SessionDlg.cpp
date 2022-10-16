#include "pch.h"
#include "SessionDlg.h"
#include <ThemeHelper.h>

LRESULT CSessionDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	CComboBox cb(GetDlgItem(IDC_LEVEL));

	PCWSTR levels[] = {
		L"All Events",
		L"Critical",
		L"Error",
		L"Warning",
		L"Information",
		L"Verbose"
	};
	for (int i = 0; i < _countof(levels); i++) {
		auto n = cb.AddString(levels[i]);
		cb.SetItemData(n, i);
	}
	cb.SetCurSel(0);

	SetDlgItemText(IDC_NAME, L"LogSession1");
	CheckDlgButton(IDC_REALTIME, BST_CHECKED);

	return 0;
}

LRESULT CSessionDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
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

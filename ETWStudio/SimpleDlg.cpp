#include "pch.h"
#include "resource.h"
#include "SimpleDlg.h"

CString const& CSimpleDlg::GetText() const {
	return m_Text;
}

LRESULT CSimpleDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	return 0;
}

LRESULT CSimpleDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	if (wID == IDOK) {
		GetDlgItemText(IDC_TEXT, m_Text);
	}
	EndDialog(wID);
	return 0;
}

#include "pch.h"
#include "resource.h"
#include "SimpleDlg.h"

CSimpleDlg::CSimpleDlg(PCWSTR title, PCWSTR text, PCWSTR cue) : m_Title(title), m_Text(text), m_Cue(cue) {
}

CString const& CSimpleDlg::GetText() const {
	return m_Text;
}

LRESULT CSimpleDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	SetWindowText(m_Title);
	SetDlgItemText(IDC_TEXT, m_Text);
	if (!m_Cue.IsEmpty())
		((CEdit)GetDlgItem(IDC_TEXT)).SetCueBannerText(m_Cue);
	return 0;
}

LRESULT CSimpleDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	if (wID == IDOK) {
		GetDlgItemText(IDC_TEXT, m_Text);
	}
	EndDialog(wID);
	return 0;
}

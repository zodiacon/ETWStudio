#include "pch.h"
#include "FindDlg.h"

CFindDlg::CFindDlg(Handler handler) : m_Handler(std::move(handler)) {
}

void CFindDlg::Show(HWND owner) {
	if (!IsWindow()) {
		Create(owner);
		CenterWindow(owner);
	}
	ShowWindow(SW_SHOW);
	SetActiveWindow();
	auto edit = GetDlgItem(IDC_FIND_TEXT);
	GotoDlgCtrl(edit);
	edit.SendMessage(EM_SETSEL, 0, -1);
}

CFindDlg::Options const& CFindDlg::GetOptions() const {
	return m_Options;
}

BOOL CFindDlg::PreTranslateMessage(MSG* pMsg) {
	if (!IsWindow() || (pMsg->hwnd != m_hWnd && !IsChild(pMsg->hwnd)))
		return FALSE;
	// F3 / Shift+F3 here too: the frame's accelerators don't get the dialog's keys
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_F3) {
		ReadOptions();
		auto options = m_Options;
		options.Forward = ::GetKeyState(VK_SHIFT) >= 0;
		if (!options.Text.empty())
			SetDlgItemText(IDC_FIND_STATUS, m_Handler(Action::FindNext, options).c_str());
		return TRUE;
	}
	return IsDialogMessage(pMsg);
}

LRESULT CFindDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	SetDialogIcon(IDI_FIND);
	SetDlgItemText(IDC_FIND_TEXT, m_Options.Text.c_str());
	CheckDlgButton(IDC_FIND_MATCHCASE, m_Options.MatchCase);
	CheckDlgButton(IDC_FIND_WHOLEWORD, m_Options.WholeWord);
	CheckDlgButton(IDC_FIND_PROPERTIES, m_Options.Properties);
	CheckRadioButton(IDC_FIND_UP, IDC_FIND_DOWN, m_Options.Forward ? IDC_FIND_DOWN : IDC_FIND_UP);
	BOOL handled;
	OnTextChanged(0, 0, nullptr, handled);
	_Module.GetMessageLoop()->AddMessageFilter(this);
	return TRUE;
}

LRESULT CFindDlg::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
	_Module.GetMessageLoop()->RemoveMessageFilter(this);
	handled = FALSE;
	return 0;
}

void CFindDlg::ReadOptions() {
	CString text;
	GetDlgItemText(IDC_FIND_TEXT, text);
	m_Options.Text = (PCWSTR)text;
	m_Options.MatchCase = IsDlgButtonChecked(IDC_FIND_MATCHCASE) == BST_CHECKED;
	m_Options.WholeWord = IsDlgButtonChecked(IDC_FIND_WHOLEWORD) == BST_CHECKED;
	m_Options.Properties = IsDlgButtonChecked(IDC_FIND_PROPERTIES) == BST_CHECKED;
	m_Options.Forward = IsDlgButtonChecked(IDC_FIND_UP) != BST_CHECKED;
}

void CFindDlg::Run(Action action) {
	ReadOptions();
	if (m_Options.Text.empty())
		return;
	SetDlgItemText(IDC_FIND_STATUS, m_Handler(action, m_Options).c_str());
}

LRESULT CFindDlg::OnFindNext(WORD, WORD, HWND, BOOL&) {
	Run(Action::FindNext);
	return 0;
}

LRESULT CFindDlg::OnSelectAll(WORD, WORD, HWND, BOOL&) {
	Run(Action::SelectAll);
	return 0;
}

LRESULT CFindDlg::OnClose(WORD, WORD, HWND, BOOL&) {
	ReadOptions();		// kept for F3
	ShowWindow(SW_HIDE);
	return 0;
}

LRESULT CFindDlg::OnTextChanged(WORD, WORD, HWND, BOOL&) {
	bool any = GetDlgItem(IDC_FIND_TEXT).GetWindowTextLength() > 0;
	GetDlgItem(IDOK).EnableWindow(any);
	GetDlgItem(IDC_FIND_SELECTALL).EnableWindow(any);
	SetDlgItemText(IDC_FIND_STATUS, L"");
	return 0;
}

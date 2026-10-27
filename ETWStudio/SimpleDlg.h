#pragma once

#include <DialogHelper.h>

class CSimpleDlg :
	public CDialogImpl<CSimpleDlg>,
	public CDialogHelper<CSimpleDlg> {
public:
	enum { IDD = IDD_SIMPLE };

	// a single-line text prompt
	explicit CSimpleDlg(PCWSTR title = L"GUID", PCWSTR text = L"", PCWSTR cue = nullptr);

	CString const& GetText() const;

protected:
	BEGIN_MSG_MAP(CSimpleDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CString m_Title, m_Text, m_Cue;
};

#pragma once

#include "resource.h"
#include <DialogHelper.h>
#include <VirtualListView.h>

struct IMainFrame;

class CSessionDlg :
	public CDialogImpl<CSessionDlg>,
	public CVirtualListView<CSessionDlg>,
	public CDialogHelper<CSessionDlg> {
public:
	enum { IDD = IDD_SESSION };

	explicit CSessionDlg(IMainFrame* frame);

	BEGIN_MSG_MAP(CSessionDlg)
		NOTIFY_HANDLER(IDC_ADD, BCN_DROPDOWN, OnProviderDropdown)
		COMMAND_ID_HANDLER(IDC_BROWSE, OnBrowseFile)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CVirtualListView<CSessionDlg>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBrowseFile(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnProviderDropdown(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

	IMainFrame* m_pFrame;
};


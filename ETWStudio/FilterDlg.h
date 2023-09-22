#pragma once

#include <DialogHelper.h>
#include <VirtualListView.h>
#include <FilterManager.h>

class CFilterDlg : 
	public CDialogImpl<CFilterDlg>,
	public CDynamicDialogLayout<CFilterDlg>,
	public CVirtualListView<CFilterDlg>,
	public CDialogHelper<CFilterDlg> {
public:
	explicit CFilterDlg(FilterManager& fm);

	enum { IDD = IDD_FILTER };

	BEGIN_MSG_MAP(CFilterDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CFilterDlg>)
	END_MSG_MAP()

private:
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	FilterManager& m_FilterMgr;
	CComboBox m_PropCB, m_CompCB, m_ValueCB;
};

// aboutdlg.h : interface of the CProvidersDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <DialogHelper.h>
#include <QuickFindEdit.h>
#include <VirtualListView.h>
#include <EtwProvider.h>
#include <SortedFilteredVector.h>

class CProvidersDlg :
	public CDialogImpl<CProvidersDlg>,
	public CVirtualListView<CProvidersDlg>,
	public CDynamicDialogLayout<CProvidersDlg>,
	public CDialogHelper<CProvidersDlg> {
public:
	enum { IDD = IDD_PROVIDERS };

	void DoSort(SortInfo const* si);
	CString GetColumnText(HWND h, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;
	bool OnDoubleClickList(HWND, int row, int col, POINT const&);
	EtwProvider const* GetSelectedProvider() const;

	BEGIN_MSG_MAP(CProvidersDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_HANDLER(IDC_FILTER, EN_DELAYCHANGE, OnChangeFilter)
		CHAIN_MSG_MAP(CVirtualListView<CProvidersDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CProvidersDlg>)
	END_MSG_MAP()

protected:
	enum class ColumnType {
		Name, Guid, Source, Count
	};

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnChangeFilter(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	CListViewCtrl m_List;
	CQuickFindEdit m_Edit;
	SortedFilteredVector<EtwProvider> m_Providers;
	EtwProvider* m_SelectedProvider{ nullptr };
	int m_OldSelected{ -1 };
	CString m_FilterText;
};

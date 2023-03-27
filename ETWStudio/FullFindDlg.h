#pragma once

#include <DialogHelper.h>
#include <VirtualListView.h>

class CFullFindDlg :
	public CDialogImpl<CFullFindDlg>,
	public CVirtualListView<CFullFindDlg>,
	public CDynamicDialogLayout<CFullFindDlg>,
	public CDialogHelper<CFullFindDlg> {
public:
	enum { IDD = IDD_FULLFIND };

	void DoSort(SortInfo const* si);
	CString GetColumnText(HWND h, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;
	bool OnDoubleClickList(HWND, int row, int col, POINT const&);

	BEGIN_MSG_MAP(CFullFindDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDC_FIND, OnFind)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		CHAIN_MSG_MAP(CVirtualListView<CFullFindDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CFullFindDlg>)
	END_MSG_MAP()

protected:
	static bool Find(std::wstring const& name, PCWSTR text);

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFind(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	struct FindItem {
		std::wstring Provider;
		std::wstring Event;
		std::wstring Property;
	};

	CListViewCtrl m_List;
	std::vector<FindItem> m_Items;
	CString m_FilterText;
	inline static BOOL m_SearchProviders { true}, m_SearchEvents{ true }, m_SearchProperties{ true };
	inline static CString m_SearchText;
};

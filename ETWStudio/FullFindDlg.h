#pragma once

#include <DialogHelper.h>
#include <VirtualListView.h>
#include <thread>
#include <atomic>

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

	~CFullFindDlg();

	BEGIN_MSG_MAP(CFullFindDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_FIND_PROGRESS, OnFindProgress)
		MESSAGE_HANDLER(WM_FIND_DONE, OnFindDone)
		COMMAND_ID_HANDLER(IDC_FIND, OnFind)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		CHAIN_MSG_MAP(CVirtualListView<CFullFindDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CFullFindDlg>)
	END_MSG_MAP()

protected:
	// posted by the search thread: wParam = providers searched, lParam = total providers
	static constexpr UINT WM_FIND_PROGRESS = WM_APP + 1;
	// posted by the search thread when it exits (finished or canceled); results are in m_Found
	static constexpr UINT WM_FIND_DONE = WM_APP + 2;

	static bool Find(std::wstring const& name, PCWSTR text);
	void Search(CString text, bool providers, bool events, bool properties);
	void StopSearch();
	void EnableSearchControls(bool enable);

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFind(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFindProgress(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnFindDone(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

private:
	struct FindItem {
		std::wstring Provider;
		std::wstring Event;
		std::wstring Property;
	};

	CListViewCtrl m_List;
	std::vector<FindItem> m_Items;		// displayed; UI thread only
	std::vector<FindItem> m_Found;		// filled by the search thread; read by the UI thread only after joining it
	std::thread m_SearchThread;
	std::atomic<bool> m_Cancel{ false };
	CString m_FilterText;
	inline static BOOL m_SearchProviders { true}, m_SearchEvents{ true }, m_SearchProperties{ true };
	inline static CString m_SearchText;
};

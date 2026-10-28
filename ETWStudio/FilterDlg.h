#pragma once

#include <DialogHelper.h>
#include <VirtualListView.h>
#include <FilterManager.h>

class PropertyFilter;

class CFilterDlg :
	public CDialogImpl<CFilterDlg>,
	public CDynamicDialogLayout<CFilterDlg>,
	public CVirtualListView<CFilterDlg>,
	public CDialogHelper<CFilterDlg> {
public:
	// Highlight: the same rules, with a row color instead of include/exclude
	enum class Mode { Filter, Highlight };
	explicit CFilterDlg(FilterManager& fm, Mode mode = Mode::Filter);

	// "Apply to captured events" checkbox
	void SetApplyToExisting(bool apply) noexcept;
	bool ApplyToExisting() const noexcept;
	// "Discard the events this filter excludes" checkbox
	void SetDiscardExcluded(bool discard) noexcept;
	bool DiscardExcluded() const noexcept;

	enum { IDD = IDD_FILTER };

	CString GetColumnText(HWND, int row, int col) const;
	ListViewRowCheck IsRowChecked(HWND, int row) const;
	void OnListViewClick(HWND, int row, int col, POINT const& pt);
	bool OnDoubleClickList(HWND, int row, int col, POINT const& pt);
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);

	BEGIN_MSG_MAP(CFilterDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_ADD, OnAdd)
		COMMAND_ID_HANDLER(IDC_UP, OnMoveUp)
		COMMAND_ID_HANDLER(IDC_DOWN, OnMoveDown)
		COMMAND_ID_HANDLER(IDC_CLEAR, OnClear)
		COMMAND_ID_HANDLER(IDC_SAVE, OnSave)
		COMMAND_ID_HANDLER(IDC_LOAD, OnLoad)
		NOTIFY_HANDLER(IDC_LIST, LVN_KEYDOWN, OnListKeyDown)
		NOTIFY_HANDLER(IDC_LIST, NM_CUSTOMDRAW, OnListCustomDraw)
		CHAIN_MSG_MAP(CVirtualListView<CFilterDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CFilterDlg>)
	END_MSG_MAP()

private:
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnMoveUp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnMoveDown(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnClear(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnLoad(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnListKeyDown(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnListCustomDraw(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

	PropertyFilter* GetFilter(int index) const;
	void ToggleFilter(int index);
	void RemoveFilter(int index);
	void Move(int delta);
	void UpdateList(int select = -1);
	void UpdateButtons();
	bool SaveFilters(PCWSTR path) const;
	bool LoadFilters(PCWSTR path);

	CListViewCtrl m_List;
	FilterManager& m_FilterMgr;
	CComboBox m_PropCB, m_CompCB, m_ValueCB, m_ActionCB;
	bool m_ApplyToExisting{ true };
	bool m_DiscardExcluded{ false };
	Mode m_Mode;
};

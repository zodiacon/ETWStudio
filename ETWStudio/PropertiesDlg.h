#pragma once

#include <DialogHelper.h>
#include <VirtualListView.h>
#include <EventData.h>
#include <optional>

class CPropertiesDlg :
	public CDialogImpl<CPropertiesDlg>,
	public CDynamicDialogLayout<CPropertiesDlg>,
	public CVirtualListView<CPropertiesDlg>,
	public CDialogHelper<CPropertiesDlg> {
public:
	// session: whose image events resolve the stack frames' symbols
	CPropertiesDlg(TraceSession const* session, std::vector<EventData*> const& events, int index, HICON hIcon);

	void OnFinalMessage(HWND) override;

	CString GetColumnText(HWND, int row, int col) const;
	bool IsSortable(HWND, int col) const;

	enum { IDD = IDD_PROPERTIES };

	BEGIN_MSG_MAP(CPropertiesDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_NEXT, OnNextEvent)
		COMMAND_ID_HANDLER(IDC_PREV, OnPrevEvent)
		CHAIN_MSG_MAP(CVirtualListView<CPropertiesDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CPropertiesDlg>)
	END_MSG_MAP()

private:
	void UpdateEvent();
	CString GetFullMessage() const;

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnNextEvent(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPrevEvent(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	TraceSession const* m_Session;
	std::vector<EventData*> const& m_Events;
	EventData* m_Data;
	// stack frames are shown as extra rows after the properties
	std::vector<ULONG64> m_Stack;
	// symbols looked up so far (empty: no module known)
	mutable std::vector<std::optional<CString>> m_StackSymbols;
	int m_Index;
	HICON m_hIcon;
};

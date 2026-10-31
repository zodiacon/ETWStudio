#pragma once

#include <DialogHelper.h>
#include <VirtualListView.h>
#include <EventData.h>
#include <optional>
#include <mutex>

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
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);

	enum { IDD = IDD_PROPERTIES };
	// posted by the symbolizer's thread when more of the stack's symbols are in
	static const UINT WM_SYMBOLS_READY = WM_APP + 10;

	BEGIN_MSG_MAP(CPropertiesDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_SYMBOLS_READY, OnSymbolsReady)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_NEXT, OnNextEvent)
		COMMAND_ID_HANDLER(IDC_PREV, OnPrevEvent)
		COMMAND_ID_HANDLER(IDC_COPY, OnCopy)
		COMMAND_ID_HANDLER(IDC_COPYALL, OnCopyAll)
		NOTIFY_HANDLER(IDC_LIST, LVN_KEYDOWN, OnListKeyDown)
		NOTIFY_HANDLER(IDC_LIST, NM_CUSTOMDRAW, OnListCustomDraw)
		CHAIN_MSG_MAP(CVirtualListView<CPropertiesDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CPropertiesDlg>)
	END_MSG_MAP()

private:
	void UpdateEvent();
	CString GetFullMessage() const;
	// the event's fields (as shown above the list), then its properties and stack frames, tab separated
	std::wstring GetAllText() const;

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSymbolsReady(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnNextEvent(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPrevEvent(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCopyAll(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnListKeyDown(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnListCustomDraw(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	TraceSession const* m_Session;
	std::vector<EventData*> const& m_Events;
	EventData* m_Data;
	// stack frames are shown as extra rows after the properties
	std::vector<ULONG64> m_Stack;
	// the stack's symbols, filled in by the symbolizer's thread (empty: no module known); shared with it, as a
	// lookup under way may still finish after the dialog moved to another event or closed
	struct StackSymbols {
		std::mutex Lock;
		std::vector<std::optional<std::wstring>> Symbols;
		std::atomic<bool> Posted{ false };	// a WM_SYMBOLS_READY is on its way
	};
	std::shared_ptr<StackSymbols> m_StackSymbols;
	int m_Index;
	HICON m_hIcon;
};

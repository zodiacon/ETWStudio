#pragma once

#include <DialogHelper.h>
#include <VirtualListView.h>
#include <EventData.h>

class CPropertiesDlg :
	public CDialogImpl<CPropertiesDlg>,
	public CDynamicDialogLayout<CPropertiesDlg>,
	public CVirtualListView<CPropertiesDlg>,
	public CDialogHelper<CPropertiesDlg> {
public:
	CPropertiesDlg(std::vector<std::shared_ptr<EventData>> const& events, int index, HICON hIcon);

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
	std::vector<std::shared_ptr<EventData>> const& m_Events;
	std::shared_ptr<EventData> m_Data;
	int m_Index;
	HICON m_hIcon;
};

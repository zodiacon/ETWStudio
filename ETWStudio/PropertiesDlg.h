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
	CPropertiesDlg(std::shared_ptr<EventData> data, HICON hIcon);

	void OnFinalMessage(HWND) override;

	CString GetColumnText(HWND, int row, int col) const;
	bool IsSortable(HWND, int col) const;

	enum { IDD = IDD_PROPERTIES };

	BEGIN_MSG_MAP(CPropertiesDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		CHAIN_MSG_MAP(CVirtualListView<CPropertiesDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CPropertiesDlg>)
	END_MSG_MAP()

private:
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	std::shared_ptr<EventData> m_Data;
	std::vector<EventProperty> m_Properties;
	HICON m_hIcon;
};

#pragma once

#include "resource.h"
#include <DialogHelper.h>
#include <VirtualListView.h>
#include <TraceSession.h>

struct IMainFrame;

class CSessionDlg :
	public CDialogImpl<CSessionDlg>,
	public CVirtualListView<CSessionDlg>,
	public CDynamicDialogLayout<CSessionDlg>,
	public CDialogHelper<CSessionDlg> {
public:
	enum { IDD = IDD_SESSION };

	CSessionDlg(IMainFrame* frame, TraceSession& session);

	CString GetColumnText(HWND, int row, int col) const;

	BEGIN_MSG_MAP(CSessionDlg)
		NOTIFY_HANDLER(IDC_ADD, BCN_DROPDOWN, OnProviderDropdown)
		COMMAND_ID_HANDLER(ID_PROVIDER_REGISTERED, OnRegisteredProvider)
		COMMAND_ID_HANDLER(ID_PROVIDER_GUID, OnGuidProvider)
		COMMAND_ID_HANDLER(IDC_BROWSE, OnBrowseFile)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CVirtualListView<CSessionDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CSessionDlg>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	struct ProviderInfo {
		GUID Guid;
		std::wstring Name;
		UCHAR Level;
	};

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBrowseFile(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnProviderDropdown(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnRegisteredProvider(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnGuidProvider(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	IMainFrame* m_pFrame;
	TraceSession& m_Session;
	std::vector<ProviderInfo> m_Providers;
	CListViewCtrl m_List;
};


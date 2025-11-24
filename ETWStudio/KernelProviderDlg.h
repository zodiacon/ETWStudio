#pragma once

#include <DialogHelper.h>
#include "resource.h"

class CKernelProviderDlg :
	public CDialogImpl<CKernelProviderDlg>,
	public CDialogHelper<CKernelProviderDlg> {
public:
	explicit CKernelProviderDlg(ULONGLONG flags = 0);

	enum { IDD = IDD_KERNELPROVIDER };

	ULONGLONG GetKernelFlags() const;

	BEGIN_MSG_MAP(CKernelProviderDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
	END_MSG_MAP()

protected:
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	struct KernelFlag {
		CButton Checkbox;
		ULONGLONG Flag;
	};
	std::vector<KernelFlag> m_KernelFlags;
};


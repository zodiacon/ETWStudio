#pragma once

#include <DialogHelper.h>
#include <KernelEvents.h>
#include <vector>
#include "resource.h"

class CKernelProviderDlg :
	public CDialogImpl<CKernelProviderDlg>,
	public CDialogHelper<CKernelProviderDlg> {
public:
	explicit CKernelProviderDlg(std::vector<KernelEventTypes> const& selected = {});

	enum { IDD = IDD_KERNELPROVIDER };

	std::vector<KernelEventTypes> const& GetSelectedTypes() const;

	BEGIN_MSG_MAP(CKernelProviderDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
	END_MSG_MAP()

protected:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	struct KernelFlag {
		CButton Checkbox;
		KernelEventTypes Type;
	};
	std::vector<KernelEventTypes> m_Initial;
	std::vector<KernelEventTypes> m_Selected;
	std::vector<KernelFlag> m_KernelFlags;
};

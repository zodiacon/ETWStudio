#include "pch.h"
#include "KernelProviderDlg.h"
#include <KernelEvents.h>

CKernelProviderDlg::CKernelProviderDlg(ULONGLONG flags) {
}

LRESULT CKernelProviderDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    int x = 30, y = 20;
    int width = 100, height = 24;
    SetDialogIcon(IDI_KERNEL);

    int n = 0;
    for (auto& evt : KernelEventCategory::GetAllCategories()) {
        CButton b;
        CRect rc(CPoint(x, y), CSize(width, height));
        b.Create(m_hWnd, rc, evt.Name.c_str(), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX);
        b.SetFont(GetFont());
        x += width + 20;
        if (++n == 4) {
            n = 0;
            x = 30;
            y += 34;
        }
    }

    return LRESULT();
}

LRESULT CKernelProviderDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
    EndDialog(wID);
    return 0;
}

#include "pch.h"
#include "KernelProviderDlg.h"
#include <algorithm>

CKernelProviderDlg::CKernelProviderDlg(std::vector<KernelEventTypes> const& selected) : m_Initial(selected) {
}

LRESULT CKernelProviderDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    SetDialogIcon(IDI_KERNEL);

    const int cols = 3;
    const int x0 = 15, y0 = 15, width = 160, height = 20, xgap = 8, ygap = 8;
    int x = x0, y = y0, n = 0;
    auto font = GetFont();

    auto& categories = KernelEventCategory::GetAllCategories();
    m_KernelFlags.reserve(categories.size());
    for (auto& cat : categories) {
        CButton b;
        CRect rc(CPoint(x, y), CSize(width, height));
        b.Create(m_hWnd, rc, cat.Name.c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX);
        b.SetFont(font);
        bool checked = std::find(m_Initial.begin(), m_Initial.end(), cat.EnableFlag) != m_Initial.end();
        b.SetCheck(checked ? BST_CHECKED : BST_UNCHECKED);
        m_KernelFlags.push_back({ b, cat.EnableFlag });

        x += width + xgap;
        if (++n == cols) {
            n = 0;
            x = x0;
            y += height + ygap;
        }
    }

    return TRUE;
}

LRESULT CKernelProviderDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
    if (wID == IDOK) {
        m_Selected.clear();
        for (auto& kf : m_KernelFlags) {
            if (kf.Checkbox.GetCheck() == BST_CHECKED &&
                std::find(m_Selected.begin(), m_Selected.end(), kf.Type) == m_Selected.end())
                m_Selected.push_back(kf.Type);
        }
    }
    EndDialog(wID);
    return 0;
}

std::vector<KernelEventTypes> const& CKernelProviderDlg::GetSelectedTypes() const {
    return m_Selected;
}

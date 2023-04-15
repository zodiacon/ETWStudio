#include "pch.h"
#include "resource.h"
#include "PropertiesDlg.h"
#include "StringHelper.h"
#include <EtwProvider.h>

CPropertiesDlg::CPropertiesDlg(std::shared_ptr<EventData> data, HICON hIcon) : 
    m_Data(std::move(data)), m_hIcon(hIcon), m_Properties(m_Data->GetProperties()) {
}

void CPropertiesDlg::OnFinalMessage(HWND) {
    delete this;
}

CString CPropertiesDlg::GetColumnText(HWND, int row, int col) const {
    switch (col) {
        case 0: return m_Properties[row].Name.c_str();
        case 1: return m_Data->FormatProperty(m_Properties[row]).c_str();
    }
    return CString();
}

bool CPropertiesDlg::IsSortable(HWND, int col) const {
    return col == 0;
}

CString CPropertiesDlg::GetFullMessage() const {
    CString msg = m_Data->GetEventStrings().Message.c_str();
    if (msg.IsEmpty())
        return msg;

    auto& props = m_Data->GetProperties();
    for (int i = 0; i < (int)m_Data->GetProperties().size(); i++) {
        msg.Replace(std::format(L"%{}", i + 1).c_str(), m_Data->FormatProperty(props[i]).c_str());
    }
    return msg;
}

LRESULT CPropertiesDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    InitDynamicLayout();
    m_List.Attach(GetDlgItem(IDC_LIST));
    m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    SetWindowText(std::format(L"Event {}", m_Data->GetIndex()).c_str());
    SetDialogIcon(m_hIcon);

    SetDlgItemInt(IDC_PID, m_Data->GetProcessId(), FALSE);
    SetDlgItemInt(IDC_TID, m_Data->GetThreadId(), FALSE);
    SetDlgItemText(IDC_PNAME, m_Data->GetProcessName().c_str());
    SetDlgItemInt(IDC_INDEX, m_Data->GetIndex(), FALSE);
    SetDlgItemText(IDC_TIME, StringHelper::TimeStampToString(m_Data->GetTimeStamp()).c_str());
    SetDlgItemText(IDC_LEVEL, m_Data->GetEventStrings().Level.c_str());
    SetDlgItemText(IDC_KEYWORD, m_Data->GetEventStrings().Keyword.c_str());
    SetDlgItemText(IDC_OPCODE, m_Data->GetEventStrings().Opcode.c_str());
    SetDlgItemText(IDC_CHANNEL, m_Data->GetEventStrings().Channel.c_str());
    SetDlgItemText(IDC_MESSAGE, GetFullMessage());
    SetDlgItemText(IDC_TASK, m_Data->GetEventStrings().Task.c_str());
    SetDlgItemText(IDC_EVENT, m_Data->GetEventStrings().Name.c_str());
    SetDlgItemText(IDC_PROVIDER, StringHelper::ProviderGuidToName(m_Data->GetProviderId()).c_str());
    SetDlgItemInt(IDC_VERSION, m_Data->GetEventDescriptor().Version);

    auto cm = GetColumnManager(m_List);
    cm->AddColumn(L"Property", 0, 150);
    cm->AddColumn(L"Value", 0, 340);

    m_List.SetItemCount((int)m_Data->GetProperties().size());

    return 0;
}

LRESULT CPropertiesDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
    DestroyWindow();
    return 0;
}

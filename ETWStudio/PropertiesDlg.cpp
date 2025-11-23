#include "pch.h"
#include "resource.h"
#include "PropertiesDlg.h"
#include "StringHelper.h"
#include <EtwProvider.h>
#include "AppSettings.h"

CPropertiesDlg::CPropertiesDlg(std::vector<std::shared_ptr<EventData>> const& events, int index, HICON hIcon) :
    m_Events(events), m_Index(index), m_hIcon(hIcon) {
}

void CPropertiesDlg::OnFinalMessage(HWND) {
    delete this;
}

CString CPropertiesDlg::GetColumnText(HWND, int row, int col) const {
    auto& props = m_Data->GetProperties();
    switch (col) {
        case 0: return props[row].Name.c_str();
        case 1: return m_Data->FormatProperty(props[row]).c_str();
    }
    return CString();
}

bool CPropertiesDlg::IsSortable(HWND, int col) const {
    return col == 0;
}

void CPropertiesDlg::UpdateEvent() {
    m_Data = m_Events[m_Index];
    auto& props = m_Data->GetProperties();

    SetWindowText(std::format(L"Event {}", m_Data->GetIndex()).c_str());

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

    m_List.SetItemCount((int)m_Data->GetProperties().size());
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
    AppSettings::Get().LoadWindowPosition(m_hWnd, L"PropertiesDialog");
    SetDialogIcon(m_hIcon);
    m_List.Attach(GetDlgItem(IDC_LIST));
    m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    GetDlgItem(IDC_NEXT).SendMessage(BM_SETIMAGE, IMAGE_ICON, (LPARAM)AtlLoadIconImage(IDI_RIGHT, 0, 24, 24));
    GetDlgItem(IDC_PREV).SendMessage(BM_SETIMAGE, IMAGE_ICON, (LPARAM)AtlLoadIconImage(IDI_LEFT, 0, 24, 24));

    auto cm = GetColumnManager(m_List);
    cm->AddColumn(L"Property", 0, 150);
    cm->AddColumn(L"Value", 0, 340);

    UpdateEvent();

    return 0;
}

LRESULT CPropertiesDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
    DestroyWindow();
    return 0;
}

LRESULT CPropertiesDlg::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
    AppSettings::Get().SaveWindowPosition(m_hWnd, L"PropertiesDialog");

    return 0;
}

LRESULT CPropertiesDlg::OnNextEvent(WORD, WORD wID, HWND, BOOL&) {
    if (m_Index == m_Events.size() - 1)
        return 0;

    m_Index++;
    UpdateEvent();
    return 0;
}

LRESULT CPropertiesDlg::OnPrevEvent(WORD, WORD wID, HWND, BOOL&) {
    if (m_Index == 0)
        return 0;

    m_Index--;
    UpdateEvent();
    return 0;
}

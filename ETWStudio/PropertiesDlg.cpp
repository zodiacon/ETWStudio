#include "pch.h"
#include "resource.h"
#include "PropertiesDlg.h"
#include "StringHelper.h"
#include <EtwProvider.h>
#include "AppSettings.h"
#include "StackSymbolizer.h"
#include <ClipboardHelper.h>
#include <ListViewHelper.h>

CPropertiesDlg::CPropertiesDlg(TraceSession const* session, std::vector<EventData*> const& events, int index, HICON hIcon) :
    m_Session(session), m_Events(events), m_Index(index), m_hIcon(hIcon) {
}

void CPropertiesDlg::OnFinalMessage(HWND) {
    delete this;
}

CString CPropertiesDlg::GetColumnText(HWND, int row, int col) const {
    auto& props = m_Data->GetProperties();
    if (row >= (int)props.size()) {
        auto frame = row - (int)props.size();
        switch (col) {
            case 0: return std::format(L"Stack [{}]", frame).c_str();
            case 1: return std::format(L"0x{:X}", m_Stack[frame]).c_str();
            case 2:
            {
                std::lock_guard locker(m_StackSymbols->Lock);
                auto& symbol = m_StackSymbols->Symbols[frame];
                return symbol ? symbol->c_str() : L"Looking up...";
            }
        }
        return CString();
    }
    switch (col) {
        case 0: return props[row].Name.c_str();
        case 1: return m_Data->FormatProperty(props[row]).c_str();
    }
    return CString();
}

bool CPropertiesDlg::IsSortable(HWND, int col) const {
    return col == 0;
}

void CPropertiesDlg::OnStateChanged(HWND, int, int, UINT oldState, UINT newState) {
    if ((oldState ^ newState) & LVIS_SELECTED)
        GetDlgItem(IDC_COPY).EnableWindow(m_List.GetSelectedCount() > 0);
}

std::wstring CPropertiesDlg::GetAllText() const {
    std::wstring text;
    // the fields in the order they're laid out
    std::pair<PCWSTR, int> const fields[] = {
        { L"Index", IDC_INDEX }, { L"Time", IDC_TIME }, { L"Provider", IDC_PROVIDER },
        { L"PID", IDC_PID }, { L"Process Name", IDC_PNAME }, { L"TID", IDC_TID },
        { L"Keyword", IDC_KEYWORD }, { L"Channel", IDC_CHANNEL },
        { L"Opcode", IDC_OPCODE }, { L"Level", IDC_LEVEL }, { L"Version", IDC_VERSION },
        { L"Task", IDC_TASK }, { L"Event", IDC_EVENT }, { L"Message", IDC_MESSAGE },
    };
    for (auto& [name, id] : fields) {
        CString value;
        GetDlgItemText(id, value);
        text += std::format(L"{}:\t{}\r\n", name, (PCWSTR)value);
    }
    text += L"\r\n";
    text += (PCWSTR)ListViewHelper::GetAllRowsAsString(m_List);
    return text;
}

void CPropertiesDlg::UpdateEvent() {
    m_Data = m_Events[m_Index];
    auto& props = m_Data->GetProperties();

    SetWindowText(std::format(L"Event {}", m_Data->GetIndex()).c_str());

    if((int)m_Data->GetProcessId() >= 0)
        SetDlgItemInt(IDC_PID, m_Data->GetProcessId());
    if ((int)m_Data->GetThreadId() >= 0)
        SetDlgItemInt(IDC_TID, m_Data->GetThreadId());
    SetDlgItemText(IDC_PNAME, m_Data->GetProcessName().c_str());
    SetDlgItemInt(IDC_INDEX, m_Data->GetIndex(), FALSE);
    SetDlgItemText(IDC_TIME, StringHelper::TimeStampToString(m_Data->GetTimeStamp()).c_str());
    SetDlgItemText(IDC_LEVEL, m_Data->GetEventStrings().Level.empty() ? StringHelper::LevelToString(m_Data->GetEventDescriptor().Level) : m_Data->GetEventStrings().Level.c_str());
    SetDlgItemText(IDC_KEYWORD, m_Data->GetEventStrings().Keyword.c_str());
    SetDlgItemText(IDC_OPCODE, m_Data->GetEventStrings().Opcode.c_str());
    SetDlgItemText(IDC_CHANNEL, m_Data->GetEventStrings().Channel.c_str());
    SetDlgItemText(IDC_MESSAGE, GetFullMessage());
    SetDlgItemText(IDC_TASK, m_Data->GetEventStrings().Task.c_str());
    SetDlgItemText(IDC_EVENT, m_Data->GetEventStrings().Name.c_str());
    SetDlgItemText(IDC_PROVIDER, StringHelper::ProviderGuidToName(m_Data->GetProviderId()).c_str());
    SetDlgItemInt(IDC_VERSION, m_Data->GetEventDescriptor().Version);

    m_Stack = m_Data->GetStackTrace();
    // the symbols come in the background: the first lookups in a module may download it and its symbols
    auto& symbolizer = StackSymbolizer::Get();
    symbolizer.Cancel(this);
    m_StackSymbols = std::make_shared<StackSymbols>();
    m_StackSymbols->Symbols.resize(m_Stack.size());
    if (!m_Stack.empty()) {
        std::vector<StackSymbolizer::Frame> frames;
        for (auto address : m_Stack)
            frames.push_back(StackSymbolizer::MakeFrame(m_Session, m_Data->GetProcessId(), address, (LONGLONG)m_Data->GetTimeStamp()));
        symbolizer.Request(this, m_Session, std::move(frames), [symbols = m_StackSymbols, hWnd = m_hWnd](size_t i, std::wstring const& symbol) {
            {
                std::lock_guard locker(symbols->Lock);
                symbols->Symbols[i] = symbol;
            }
            if (!symbols->Posted.exchange(true))
                ::PostMessage(hWnd, WM_SYMBOLS_READY, 0, 0);
        });
    }
    m_List.SetItemCount((int)(props.size() + m_Stack.size()));
    GetDlgItem(IDC_COPY).EnableWindow(m_List.GetSelectedCount() > 0);
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
    cm->AddColumn(L"Value", 0, 200);
    cm->AddColumn(L"Details", 0, 300);	// the symbol of a stack frame
    GetDlgItem(IDC_COPY).EnableWindow(FALSE);	// until rows are selected

    UpdateEvent();

    return 0;
}

LRESULT CPropertiesDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
    DestroyWindow();
    return 0;
}

LRESULT CPropertiesDlg::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
    StackSymbolizer::Get().Cancel(this);
    AppSettings::Get().SaveWindowPosition(m_hWnd, L"PropertiesDialog");

    return 0;
}

LRESULT CPropertiesDlg::OnSymbolsReady(UINT, WPARAM, LPARAM, BOOL&) {
    // (may be from the previous event's lookup: then there's just nothing new)
    m_StackSymbols->Posted = false;
    if (auto first = (int)m_Data->GetProperties().size(); !m_Stack.empty())
        m_List.RedrawItems(first, first + (int)m_Stack.size() - 1);
    return 0;
}

LRESULT CPropertiesDlg::OnNextEvent(WORD, WORD wID, HWND, BOOL&) {
    if (m_Index == m_Events.size() - 1)
        return 0;

    m_Index++;
    UpdateEvent();
    return 0;
}

LRESULT CPropertiesDlg::OnCopy(WORD, WORD wID, HWND, BOOL&) {
    if (m_List.GetSelectedCount() > 0)
        ClipboardHelper::CopyText(m_hWnd, ListViewHelper::GetSelectedRowsAsString(m_List));
    return 0;
}

LRESULT CPropertiesDlg::OnCopyAll(WORD, WORD wID, HWND, BOOL&) {
    ClipboardHelper::CopyText(m_hWnd, GetAllText().c_str());
    return 0;
}

LRESULT CPropertiesDlg::OnListKeyDown(int, LPNMHDR hdr, BOOL& handled) {
    auto key = ((NMLVKEYDOWN*)hdr)->wVKey;
    if (::GetKeyState(VK_CONTROL) < 0 && (key == 'C' || key == 'A')) {
        if (key == 'C')
            SendMessage(WM_COMMAND, IDC_COPY);
        else
            m_List.SetItemState(-1, LVIS_SELECTED, LVIS_SELECTED);
        return 0;
    }
    handled = FALSE;
    return 0;
}

LRESULT CPropertiesDlg::OnListCustomDraw(int, LPNMHDR hdr, BOOL&) {
    auto cd = (NMLVCUSTOMDRAW*)hdr;
    switch (cd->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;

        case CDDS_ITEMPREPAINT:
            // selected rows look as selected without the focus too (the theme's inactive selection is faint, in
            // dark mode almost invisible): the rows Copy copies while a button has the focus
            if (::GetFocus() != m_List && m_List.GetItemState((int)cd->nmcd.dwItemSpec, LVIS_SELECTED)) {
                cd->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS);
                cd->clrTextBk = ::GetSysColor(COLOR_HIGHLIGHT);
                cd->clrText = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
                return CDRF_NEWFONT;
            }
            break;
    }
    return CDRF_DODEFAULT;
}

LRESULT CPropertiesDlg::OnPrevEvent(WORD, WORD wID, HWND, BOOL&) {
    if (m_Index == 0)
        return 0;

    m_Index--;
    UpdateEvent();
    return 0;
}

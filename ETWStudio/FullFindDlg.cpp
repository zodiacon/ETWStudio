#include "pch.h"
#include "resource.h"
#include "FullFindDlg.h"
#include <EtwProvider.h>

void CFullFindDlg::DoSort(SortInfo const* si) {
}

CString CFullFindDlg::GetColumnText(HWND h, int row, int col) const {
	auto& item = m_Items[row];
	switch (col) {
		case 0: return item.Provider.c_str();
		case 1: return item.Event.c_str();
		case 2: return item.Property.c_str();
	}
	return CString();
}

int CFullFindDlg::GetRowImage(HWND, int row, int col) const {
	return 0;
}

bool CFullFindDlg::OnDoubleClickList(HWND, int row, int col, POINT const&) {
	return false;
}

bool CFullFindDlg::Find(std::wstring const& name, PCWSTR text) {
	if (name.empty())
		return false;

	CString sname(name.c_str());
	sname.MakeUpper();
	return sname.Find(text) >= 0;
}

LRESULT CFullFindDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	m_List.Attach(GetDlgItem(IDC_LIST));
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

	CheckDlgButton(IDC_PROVIDER, m_SearchProviders);
	CheckDlgButton(IDC_EVENTS, m_SearchEvents);
	CheckDlgButton(IDC_PROPS, m_SearchProperties);
	SetDlgItemText(IDC_TEXT, m_SearchText);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Provider", LVCFMT_LEFT, 150);
	cm->AddColumn(L"Event", LVCFMT_LEFT, 250);
	cm->AddColumn(L"Property", LVCFMT_LEFT, 150);

	return 0;
}

LRESULT CFullFindDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	EndDialog(wID);
	return 0;
}

LRESULT CFullFindDlg::OnFind(WORD, WORD wID, HWND, BOOL&) {
	m_SearchProviders = IsDlgButtonChecked(IDC_PROVIDER) == BST_CHECKED;
	m_SearchEvents = IsDlgButtonChecked(IDC_EVENTS) == BST_CHECKED;
	m_SearchProperties = IsDlgButtonChecked(IDC_PROPS) == BST_CHECKED;
	GetDlgItemText(IDC_TEXT, m_SearchText);
	CString text(m_SearchText);
	text.MakeUpper();

	m_Items.clear();
	m_List.SetItemCount(0);
	SetDlgItemText(IDC_STATS, L"");
	CWaitCursor wait;

	for (auto& provider : EtwProvider::EnumProviders()) {
		if (m_SearchProviders) {
			CString name(provider.Name().c_str());
			name.MakeUpper();
			if (name.Find(text) >= 0) {
				FindItem item;
				item.Provider = provider.Name();
				m_Items.push_back(std::move(item));
			}
			else if (name = provider.GuidAsString().c_str(); name.Find(text) >= 0) {
				FindItem item;
				item.Provider = provider.GuidAsString();
				m_Items.push_back(std::move(item));
			}
		}
		if (m_SearchEvents || m_SearchProperties) {
			for (auto& desc : provider.GetProviderEvents()) {
				auto evt = provider.EventInfo(desc);
				if (m_SearchEvents) {
					if (Find(evt.EventName, text)) {
						FindItem item;
						item.Provider = provider.Name();
						item.Event = L"Name: " + evt.EventName;
						m_Items.push_back(std::move(item));
					}
					if (Find(evt.KeywordName, text)) {
						FindItem item;
						item.Provider = provider.Name();
						item.Event = L"Keyword: " + evt.KeywordName;
						m_Items.push_back(std::move(item));
					}
					if (Find(evt.EventMessage, text)) {
						FindItem item;
						item.Provider = provider.Name();
						item.Event = L"Message: " + evt.EventMessage;
						m_Items.push_back(std::move(item));
					}
					if (Find(evt.ChannelName, text)) {
						FindItem item;
						item.Provider = provider.Name();
						item.Event = L"Channel: " + evt.ChannelName;
						m_Items.push_back(std::move(item));
					}
					if (Find(evt.OpCodeName, text)) {
						FindItem item;
						item.Provider = provider.Name();
						item.Event = L"Opcode: " + evt.OpCodeName;
						m_Items.push_back(std::move(item));
					}
				}
				if (m_SearchProperties) {
					for (auto& prop : evt.Properties) {
						if (Find(prop.Name, text)) {
							FindItem item;
							item.Provider = provider.Name();
							item.Event = L"Event: " 
								+ (evt.ChannelName.empty() ? L"" : (L"Channel: " + evt.ChannelName + L" "))
								+ (evt.TaskName.empty() ? L"" : (L"Task: " + evt.TaskName + L" "))
								+ (evt.OpCodeName.empty() ? L"" : (L"Opcode: " + evt.OpCodeName + L" "));
							item.Property = prop.Name;
							m_Items.push_back(std::move(item));
						}
					}
				}
			}
		}
	}
	m_List.SetItemCount((int)m_Items.size());
	SetDlgItemText(IDC_STATS, std::format(L"Items found: {}", m_Items.size()).c_str());

	return 0;
}

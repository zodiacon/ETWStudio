#include "pch.h"
#include "resource.h"
#include "FullFindDlg.h"
#include <EtwProvider.h>
#include <SortHelper.h>

void CFullFindDlg::DoSort(SortInfo const* si) {
	auto compare = [&](auto& i1, auto& i2) {
		switch (si->SortColumn) {
			case 0: return SortHelper::Sort(i1.Provider, i2.Provider, si->SortAscending);
			case 1: return SortHelper::Sort(i1.Event, i2.Event, si->SortAscending);
			case 2: return SortHelper::Sort(i1.Property, i2.Property, si->SortAscending);
		}
		return false;
	};
	std::sort(m_Items.begin(), m_Items.end(), compare);
}

CString CFullFindDlg::GetColumnText(HWND h, int row, int col) const {
	auto& item = m_Items[row];
	switch (col) {
		case 0: return item.Provider.c_str();
		case 1: return item.Event.c_str();
		case 2: return item.Property.c_str();
	}
	return L"";
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
	SetDialogIcon(IDI_PROVIDERS);

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

CFullFindDlg::~CFullFindDlg() {
	StopSearch();
}

LRESULT CFullFindDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	StopSearch();
	EndDialog(wID);
	return 0;
}

void CFullFindDlg::StopSearch() {
	if (m_SearchThread.joinable()) {
		m_Cancel = true;
		m_SearchThread.join();
	}
}

void CFullFindDlg::EnableSearchControls(bool enable) {
	for (auto id : { IDC_TEXT, IDC_PROVIDER, IDC_EVENTS, IDC_PROPS })
		GetDlgItem(id).EnableWindow(enable);
	SetDlgItemText(IDC_FIND, enable ? L"Find" : L"Stop");
}

LRESULT CFullFindDlg::OnFind(WORD, WORD wID, HWND, BOOL&) {
	if (m_SearchThread.joinable()) {
		// "Stop": the thread posts WM_FIND_DONE on its way out, which shows the partial results
		m_Cancel = true;
		return 0;
	}

	m_SearchProviders = IsDlgButtonChecked(IDC_PROVIDER) == BST_CHECKED;
	m_SearchEvents = IsDlgButtonChecked(IDC_EVENTS) == BST_CHECKED;
	m_SearchProperties = IsDlgButtonChecked(IDC_PROPS) == BST_CHECKED;
	GetDlgItemText(IDC_TEXT, m_SearchText);
	CString text(m_SearchText);
	text.MakeUpper();

	m_Items.clear();
	m_List.SetItemCount(0);
	m_Found.clear();
	SetDlgItemText(IDC_STATS, L"Searching...");
	EnableSearchControls(false);

	m_Cancel = false;
	m_SearchThread = std::thread([=, this, providers = (bool)m_SearchProviders,
		events = (bool)m_SearchEvents, props = (bool)m_SearchProperties] {
		// MOF providers are decoded through WMI, which needs COM on this thread
		auto hr = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		Search(text, providers, events, props);
		if (SUCCEEDED(hr))
			::CoUninitialize();
		PostMessage(WM_FIND_DONE, m_Cancel ? 1 : 0);
		});

	return 0;
}

LRESULT CFullFindDlg::OnFindProgress(UINT, WPARAM done, LPARAM total, BOOL&) {
	if (m_SearchThread.joinable())
		SetDlgItemText(IDC_STATS, std::format(L"Searching... {}/{}", done, total).c_str());
	return 0;
}

LRESULT CFullFindDlg::OnFindDone(UINT, WPARAM canceled, LPARAM, BOOL&) {
	if (!m_SearchThread.joinable())
		return 0;

	m_SearchThread.join();		// it's exiting; after this m_Found is ours
	m_Items = std::move(m_Found);
	m_Found.clear();
	m_List.SetItemCount((int)m_Items.size());
	SetDlgItemText(IDC_STATS, std::format(L"Items found: {}{}", m_Items.size(), canceled ? L" (stopped)" : L"").c_str());
	EnableSearchControls(true);
	GetDlgItem(IDC_TEXT).SetFocus();

	return 0;
}

//
// runs on the search thread: touches only m_Found and m_Cancel, and talks to the UI with PostMessage
//
void CFullFindDlg::Search(CString text, bool searchProviders, bool searchEvents, bool searchProperties) {
	auto providers = EtwProvider::EnumProviders();
	size_t done = 0;
	auto lastProgress = ::GetTickCount64();

	for (auto& provider : providers) {
		if (m_Cancel)
			return;

		if (auto now = ::GetTickCount64(); now - lastProgress > 200) {
			lastProgress = now;
			PostMessage(WM_FIND_PROGRESS, done, providers.size());
		}
		done++;

		if (searchProviders) {
			CString name(provider.Name().c_str());
			name.MakeUpper();
			if (name.Find(text) >= 0) {
				FindItem item;
				item.Provider = provider.Name();
				m_Found.push_back(std::move(item));
			}
			else if (name = provider.GuidAsString().c_str(); name.Find(text) >= 0) {
				FindItem item;
				item.Provider = provider.GuidAsString();
				m_Found.push_back(std::move(item));
			}
		}
		if (searchEvents || searchProperties) {
			for (auto const& desc : provider.GetProviderEvents()) {
				if (m_Cancel)
					return;
				auto const& evt = provider.EventInfo(desc);
				if (searchEvents) {
					if (Find(evt.EventName, text)) {
						FindItem item;
						item.Provider = provider.Name();
						item.Event = L"Name: " + evt.EventName;
						m_Found.push_back(std::move(item));
					}
					if (Find(evt.KeywordName, text)) {
						FindItem item;
						item.Provider = provider.Name();
						item.Event = L"Keyword: " + evt.KeywordName;
						m_Found.push_back(std::move(item));
					}
					if (Find(evt.EventMessage, text)) {
						FindItem item;
						item.Provider = provider.Name();
						item.Event = L"Message: " + evt.EventMessage;
						m_Found.push_back(std::move(item));
					}
					if (Find(evt.ChannelName, text)) {
						FindItem item;
						item.Provider = provider.Name();
						item.Event = L"Channel: " + evt.ChannelName;
						m_Found.push_back(std::move(item));
					}
					if (Find(evt.OpCodeName, text)) {
						FindItem item;
						item.Provider = provider.Name();
						item.Event = L"Opcode: " + evt.OpCodeName;
						m_Found.push_back(std::move(item));
					}
				}
				if (searchProperties) {
					for (auto& prop : evt.Properties) {
						if (Find(prop.Name, text)) {
							FindItem item;
							item.Provider = provider.Name();
							item.Event = L"Event: " 
								+ (evt.ChannelName.empty() ? L"" : (L"Channel: " + evt.ChannelName + L" "))
								+ (evt.TaskName.empty() ? L"" : (L"Task: " + evt.TaskName + L" "))
								+ (evt.OpCodeName.empty() ? L"" : (L"Opcode: " + evt.OpCodeName + L" "));
							item.Property = prop.Name;
							m_Found.push_back(std::move(item));
						}
					}
				}
			}
		}
	}
}

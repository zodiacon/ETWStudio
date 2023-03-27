// View.cpp : implementation of the CProvidersView class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "ProvidersView.h"
#include <ListViewhelper.h>
#include <SortHelper.h>
#include "StringHelpers.h"

BOOL CProvidersView::PreTranslateMessage(MSG* pMsg) {
	pMsg;
	return FALSE;
}

void CProvidersView::InitProviderList() {
	CWaitCursor wait;

	m_Providers = EtwProvider::EnumProviders();
	m_ProviderList.SetItemCount((int)m_Providers.size());
}

LRESULT CProvidersView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_hWndClient = m_Splitter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN);
	m_HSplitter.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN);
	m_EventList.Create(m_HSplitter, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | LVS_OWNERDATA | LVS_REPORT | LVS_SINGLESEL, 0, IDC_EVENTLIST);
	m_EventList.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_HEADERDRAGDROP | LVS_EX_SUBITEMIMAGES);
	m_PropList.Create(m_HSplitter, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LVS_OWNERDATA | LVS_REPORT, 0, IDC_PROPLIST);
	m_PropList.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	m_ProviderList.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LVS_OWNERDATA | LVS_REPORT | LVS_SINGLESEL, 0, IDC_PROVIDERLIST);
	m_ProviderList.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);

	auto cm = GetColumnManager(m_ProviderList);
	cm->AddColumn(L"Provider Name", 0, 240, ColumnType::Name);
	cm->AddColumn(L"Provider GUID", 0, 280, ColumnType::Guid);
	cm->AddColumn(L"Source", 0, 60, ColumnType::Type);
	cm->AddColumn(L"Events", LVCFMT_RIGHT, 80, ColumnType::Count);

	cm = GetColumnManager(m_EventList);
	cm->AddColumn(L"Level", 0, 150, ColumnType::Level);
	cm->AddColumn(L"Keyword", 0, 150, ColumnType::Keyword);
	cm->AddColumn(L"ID", LVCFMT_RIGHT, 60, ColumnType::Id);
	cm->AddColumn(L"Version", LVCFMT_RIGHT, 60, ColumnType::Version);
	cm->AddColumn(L"Task", 0, 200, ColumnType::Task);
	cm->AddColumn(L"OpCode", 0, 150, ColumnType::OpCode);
	cm->AddColumn(L"Message", 0, 300, ColumnType::Message);
	cm->AddColumn(L"Name", 0, 150, ColumnType::Name);
	cm->AddColumn(L"Properties", LVCFMT_RIGHT, 60, ColumnType::Count);
	cm->AddColumn(L"Channel", 0, 150, ColumnType::ChannelName);
	cm->AddColumn(L"GUID", 0, 250, ColumnType::Guid);

	m_QuickFind.Create(this, 2, m_EventList, rcDefault, L"", WS_CHILD | WS_BORDER, 0, 123);
	m_QuickFind.SetLimitText(20);
	m_QuickFind.SetFont(m_EventList.GetFont());
	m_QuickFind.SetWatermark(L"Type to filter (Ctrl+Q)");
	//m_QuickFind.SetHotKey(MOD_CONTROL, 'Q');

	{
		CImageList images;
		images.Create(16, 16, ILC_COLOR32, 8, 4);
		UINT icons[] = {
			IDI_COMPUTER, IDI_PROVIDER1, IDI_PROVIDER2
		};
		for (auto& icon : icons)
			images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
		m_ProviderList.SetImageList(images, LVSIL_SMALL);
	}
	{
		CImageList images;
		images.Create(16, 16, ILC_COLOR32, 8, 4);
		UINT icons[] = {
			IDI_PROVIDER1, IDI_PROVIDER2, IDI_EVENT2,
			IDI_EVENT, IDI_CRITICAL, IDI_LERROR, IDI_LWARNING, IDI_LINFO, IDI_LDEBUG,
		};
		for (auto& icon : icons)
			images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
		m_EventList.SetImageList(images, LVSIL_SMALL);
	}

	cm = GetColumnManager(m_PropList);
	cm->AddColumn(L"Name", 0, 150, ColumnType::Name);
	cm->AddColumn(L"In Type", 0, 120, ColumnType::InType);
	cm->AddColumn(L"Out Type", 0, 120, ColumnType::OutType);

	m_Splitter.SetSplitterPanes(m_ProviderList, m_HSplitter);
	m_Splitter.SetSplitterPosPct(35);
	m_HSplitter.SetSplitterPanes(m_EventList, m_PropList);
	m_HSplitter.SetSinglePaneMode(0);
	m_HSplitter.SetSplitterPosPct(75);

	InitProviderList();

	return 0;
}

LRESULT CProvidersView::OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
	m_ProviderList.SetFocus();
	return 0;
}

LRESULT CProvidersView::OnQuickFind(WORD, WORD, HWND, BOOL&) {
	if (m_QuickFind.IsWindowVisible() && ::GetFocus() == m_QuickFind)
		m_QuickFind.ShowWindow(SW_HIDE);
	else {
		CRect rc;
		m_EventList.GetClientRect(&rc);
		m_QuickFind.MoveWindow(rc.right - 150, rc.top + 40, 130, 25);
		m_QuickFind.ShowWindow(SW_SHOW);
		m_QuickFind.SetFocus();
	}
	return 0;
}

LRESULT CProvidersView::OnSize(UINT, WPARAM, LPARAM, BOOL& handled) {
	if (m_QuickFind.IsWindowVisible()) {
		CRect rc;
		m_EventList.GetClientRect(&rc);
		m_QuickFind.MoveWindow(rc.right - 150, rc.top + 40, 130, 25);
	}
	handled = FALSE;
	return 0;
}

LRESULT CProvidersView::OnQuickEditChar(UINT, WPARAM wp, LPARAM lp, BOOL& handled) {
	LRESULT result = 0;
	if (wp == VK_ESCAPE && m_QuickFind.GetWindowTextLength() == 0) {
		m_QuickFind.ShowWindow(SW_HIDE);
	}
	else {
		handled = m_QuickFind.ProcessWindowMessage(m_QuickFind, WM_CHAR, wp, lp, result);
	}
	return result;
}

CString CProvidersView::GetColumnText(HWND h, int row, int col) const {
	if (h == m_PropList)
		return GetPropertyText(row, col);

	auto tag = GetColumnManager(h)->GetColumnTag<ColumnType>(col);

	if (h == m_ProviderList) {
		auto& item = m_Providers[row];
		switch (tag) {
			case ColumnType::Name: return item.Name().c_str();
			case ColumnType::Guid: return item.GuidAsString().c_str();
			case ColumnType::Type: return item.SchemaSource() == EtwSchemaSource::Mof ? L"MOF" : L"XML";
			case ColumnType::Count: return std::to_wstring(item.EventCount()).c_str();
		}
	}
	else {
		auto index = m_ProviderList.GetSelectedIndex();
		if (index >= 0) {
			auto event = m_Providers[index].EventInfo(m_Events[row]);
			switch (tag) {
				case ColumnType::Name: return event.EventName.c_str();
				case ColumnType::Keyword: return event.KeywordName.c_str();
				case ColumnType::Id: return std::to_wstring(event.Descriptor.Id).c_str();
				case ColumnType::Task: return event.TaskName.c_str();
				case ColumnType::OpCode: return event.OpCodeName.c_str();
				case ColumnType::Level: return std::format(L"{} ({})", event.LevelName, m_Events[row].Level).c_str();
				case ColumnType::Source: return StringHelpers::DecodingSourceToString(event.DescodingSource);
				case ColumnType::Guid: return StringHelpers::GuidToString(event.EventGuid).c_str();
				case ColumnType::Message: return event.EventMessage.c_str();
				case ColumnType::Count: return std::to_wstring(event.Properties.size()).c_str();
				case ColumnType::ChannelName: return event.ChannelName.c_str();
				case ColumnType::Version: return std::to_wstring(m_Events[row].Version).c_str();
			}
		}
	}
	return L"";
}

CString CProvidersView::GetPropertyText(int row, int col) const {
	auto& pi = m_Properties[row];
	switch (GetColumnManager(m_PropList)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Name: return pi.Name.c_str();
		case ColumnType::InType: return StringHelpers::InTypeToString(pi.InType).c_str();
		case ColumnType::OutType: return StringHelpers::OutTypeToString(pi.OutType).c_str();
	}
	return CString();
}

void CProvidersView::DoSort(const SortInfo* si) {
	if (si == nullptr)
		return;

	if (si->hWnd == m_PropList)
		return DoSortProperties(si);

	auto asc = si->SortAscending;
	auto tag = GetColumnManager(si->hWnd)->GetColumnTag<ColumnType>(si->SortColumn);

	if (si->hWnd == m_ProviderList) {
		std::sort(m_Providers.begin(), m_Providers.end(), [&](const auto& a1, const auto& a2) {
			switch (tag) {
				case ColumnType::Name: return SortHelper::Sort(a1.Name(), a2.Name(), asc);
				case ColumnType::Guid: return SortHelper::Sort(a1.GuidAsString(), a2.GuidAsString(), asc);
				case ColumnType::Source: return SortHelper::Sort(a1.SchemaSource(), a2.SchemaSource(), asc);
				case ColumnType::Count: return SortHelper::Sort(a1.EventCount(), a2.EventCount(), asc);
			}
			return false;
			});
	}
	else {
		auto& provider = m_Providers[m_ProviderList.GetSelectedIndex()];
		std::sort(m_Events.begin(), m_Events.end(), [&](const auto& ev1, const auto& ev2) {
			auto e1 = provider.EventInfo(ev1);
			auto e2 = provider.EventInfo(ev2);
			switch (tag) {
				case ColumnType::Name: return SortHelper::Sort(e1.EventName, e2.EventName, asc);
				case ColumnType::Keyword: return SortHelper::Sort(e1.KeywordName, e2.KeywordName, asc);
				case ColumnType::Id: return SortHelper::Sort(e1.Descriptor.Id, e2.Descriptor.Id, asc);
				case ColumnType::OpCode: return SortHelper::Sort(e1.OpCodeName, e2.OpCodeName, asc);
				case ColumnType::Level: return SortHelper::Sort(ev1.Level, ev2.Level, asc);
				case ColumnType::Message: return SortHelper::Sort(e1.EventMessage, e2.EventMessage, asc);
				case ColumnType::Source: return SortHelper::Sort(e1.DescodingSource, e2.DescodingSource, asc);
				case ColumnType::Task: return SortHelper::Sort(e1.TaskName, e2.TaskName, asc);
				case ColumnType::Guid: return SortHelper::Sort(StringHelpers::GuidToString(e1.EventGuid), StringHelpers::GuidToString(e2.EventGuid), asc);
				case ColumnType::Count: return SortHelper::Sort(e1.Properties.size(), e2.Properties.size(), asc);
				case ColumnType::ChannelName: return SortHelper::Sort(e1.ChannelName, e2.ChannelName, asc);
				case ColumnType::Version: return SortHelper::Sort(ev1.Version, ev2.Version, asc);
			}
			return false;
			});
	}
}

void CProvidersView::DoSortProperties(const SortInfo* si) {
	auto cm = GetColumnManager(m_PropList);
	auto asc = si->SortAscending;
	auto col = cm->GetColumnTag<ColumnType>(si->SortColumn);
	auto compare = [&](auto const& p1, auto const& p2) {
		switch (col) {
			case ColumnType::Name: return SortHelper::Sort(p1.Name, p2.Name, asc);
			case ColumnType::InType: return SortHelper::Sort(p1.InType, p2.InType, asc);
			case ColumnType::OutType: return SortHelper::Sort(p1.OutType, p2.OutType, asc);
		}
		return false;
	};
	std::ranges::sort(m_Properties, compare);
}

int CProvidersView::GetRowImage(HWND h, int row, int col) const {
	if (h == m_PropList)
		return -1;

	if (h == m_ProviderList) {
		return m_Providers[row].SchemaSource() == EtwSchemaSource::Xml ? 1 : 0;
	}
	else if (GetColumnManager(h)->GetColumnTag<ColumnType>(col) == ColumnType::Level) {
		return 3 + m_Events[row].Level;
	}
	return -1;
}

void CProvidersView::OnStateChanged(HWND h, int from, int to, UINT oldState, UINT newState) {
	if (h == m_EventList && (newState & LVIS_SELECTED) && to == from) {
		//
		// get properties of selected event
		//
		int index = m_ProviderList.GetSelectedIndex();
		if (index < 0) {
			m_HSplitter.SetSinglePaneMode(0);
		}
		else {
			m_Properties = m_Providers[index].EventInfo(m_Events[from]).Properties;
			m_PropList.SetItemCount((int)m_Properties.size());
			Sort(m_PropList);
			m_HSplitter.SetSinglePaneMode(-1);
		}
	}
	else if (h == m_ProviderList && (newState & LVIS_SELECTED)) {
		int index = m_ProviderList.GetSelectedIndex();
		if (index < 0) {
			m_Events.clear();
			m_EventList.SetItemCount(0);
		}
		else {
			m_Events = m_Providers[index].GetProviderEvents();
			m_EventList.SetItemCount((int)m_Events.size());
			Sort(m_EventList);
		}
		m_HSplitter.SetSinglePaneMode(0);
	}
}


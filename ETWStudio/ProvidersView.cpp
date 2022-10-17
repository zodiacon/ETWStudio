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

void CProvidersView::InitTree() {
	CWaitCursor wait;
	m_Tree.SetRedraw(FALSE);

	WCHAR computer[MAX_COMPUTERNAME_LENGTH];
	DWORD len = _countof(computer);
	::GetComputerName(computer, &len);
	auto root = InsertTreeItem(m_Tree, computer, TreeIconType::Computer, TreeItemType::Root);
	m_Providers = EtwProvider::EnumProviders2();

	m_ProvidersMap.clear();
	for (auto& p : m_Providers) {
		if (p->EventCount() == 0)
			continue;
		auto hItem = InsertTreeItem(m_Tree, std::format(L"{} ({} Events)", p->Name(), p->EventCount()).c_str(), 
			p->SchemaSource() == EtwSchemaSource::Xml ? TreeIconType::Provider2 : TreeIconType::Provider1, 
			TreeItemType::Provider, root, TVI_SORT);
		m_ProvidersMap.insert({ hItem, p.get() });
	}
	m_Tree.SelectItem(root);
	m_Tree.Expand(root, TVE_EXPAND);
	m_Tree.SetRedraw(TRUE);
}

void CProvidersView::RefreshList(bool changeHeader) {
	if (changeHeader && !LoadState(m_List, m_ListViewState[(int)m_CurrentNode])) {
		//
		// add columns based on view
		//
		auto& cm = m_CurrentNode == TreeItemType::Root ? m_ProviderCM : m_EventsCM;
		cm.Clear();

		switch (m_CurrentNode) {
			case TreeItemType::Root:
				cm.AddColumn(L"Provider Name", 0, 240, ColumnType::Name);
				cm.AddColumn(L"Provider GUID", 0, 280, ColumnType::Guid);
				cm.AddColumn(L"Source", 0, 60, ColumnType::Type);
				cm.AddColumn(L"Events", LVCFMT_RIGHT, 80, ColumnType::Count);
				break;

			case TreeItemType::Provider:
				cm.AddColumn(L"Keyword", 0, 150, ColumnType::Keyword);
				cm.AddColumn(L"ID", LVCFMT_RIGHT, 60, ColumnType::Id);
				cm.AddColumn(L"Task", 0, 200, ColumnType::Task);
				cm.AddColumn(L"OpCode", 0, 150, ColumnType::OpCode);
				cm.AddColumn(L"Level", 0, 150, ColumnType::Level);
				cm.AddColumn(L"Message", 0, 300, ColumnType::Message);
				cm.AddColumn(L"Properties", LVCFMT_RIGHT, 60, ColumnType::Count);
				cm.AddColumn(L"GUID", 0, 250, ColumnType::Guid);
				cm.AddColumn(L"Channel", 0, 150, ColumnType::ChannelName);
				cm.AddColumn(L"Name", 0, 150, ColumnType::Name);
				break;

		}
		cm.UpdateColumns();
	}

	//
	// build column data
	//
	int count = 0;
	switch (m_CurrentNode) {
		case TreeItemType::Root:
			count = (int)m_Providers.size();
			break;

		case TreeItemType::Provider:
			m_CurrentProvider = m_ProvidersMap[m_Tree.GetSelectedItem()];
			m_Events = m_CurrentProvider->GetProviderEvents();
			count = (int)m_Events.size();
			break;
	}
	m_List.SetItemCount(count);

	if (m_CurrentNode == TreeItemType::Root)
		m_HSplitter.SetSinglePaneMode(0);
	else
		m_HSplitter.SetSinglePaneMode();
	ClearSort(m_List);
}

LRESULT CProvidersView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_hWndClient = m_Splitter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN);
	m_HSplitter.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN);
	m_List.Create(m_HSplitter, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LVS_OWNERDATA | LVS_REPORT, 0, 123);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	m_PropList.Create(m_HSplitter, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LVS_OWNERDATA | LVS_REPORT, 0, 124);
	m_PropList.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	m_Tree.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
		TVS_HASLINES | TVS_LINESATROOT | TVS_HASLINES | TVS_HASBUTTONS | TVS_SHOWSELALWAYS);
	m_Tree.SetExtendedStyle(TVS_EX_DOUBLEBUFFER | TVS_EX_DRAWIMAGEASYNC, TVS_EX_DOUBLEBUFFER | TVS_EX_DRAWIMAGEASYNC);

	m_ProviderCM.Attach(m_List);
	m_EventsCM.Attach(m_List);

	{
		CImageList images;
		images.Create(16, 16, ILC_COLOR32, 8, 4);
		UINT icons[] = {
			IDI_COMPUTER, IDI_PROVIDER1, IDI_PROVIDER2
		};
		for (auto& icon : icons)
			images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
		m_Tree.SetImageList(images);
	}
	{
		CImageList images;
		images.Create(16, 16, ILC_COLOR32, 8, 4);
		UINT icons[] = {
			IDI_PROVIDER1, IDI_PROVIDER2, 
			IDI_EVENT, IDI_CRITICAL, IDI_LERROR, IDI_LWARNING, IDI_LINFO, IDI_LDEBUG,
		};
		for (auto& icon : icons)
			images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
		m_List.SetImageList(images, LVSIL_SMALL);
	}

	auto cm = GetColumnManager(m_PropList);
	cm->AddColumn(L"Name", 0, 150, ColumnType::Name);
	cm->AddColumn(L"In Type", 0, 120, ColumnType::InType);
	cm->AddColumn(L"Out Type", 0, 120, ColumnType::OutType);

	m_Splitter.SetSplitterPanes(m_Tree, m_HSplitter);
	m_Splitter.SetSplitterPosPct(30);
	m_HSplitter.SetSplitterPanes(m_List, m_PropList);
	m_HSplitter.SetSinglePaneMode(0);
	m_HSplitter.SetSplitterPosPct(75);

	InitTree();

	return 0;
}

CString CProvidersView::GetColumnText(HWND h, int row, int col) const {
	if (h == m_PropList)
		return GetPropertyText(row, col);

	auto& cm = m_CurrentNode == TreeItemType::Root ? m_ProviderCM : m_EventsCM;
	auto tag = cm.GetColumnTag<ColumnType>(col);

	switch (m_CurrentNode) {
		case TreeItemType::Root:
		{
			auto& item = m_Providers[row];
			switch (tag) {
				case ColumnType::Name: return item->Name().c_str();
				case ColumnType::Guid: return item->GuidAsString().c_str();
				case ColumnType::Type: return item->SchemaSource() == EtwSchemaSource::Mof ? L"MOF" : L"XML";
				case ColumnType::Count: return std::to_wstring(item->EventCount()).c_str();
			}
			break;
		}

		case TreeItemType::Provider:
			auto event = m_CurrentProvider->EventInfo(m_Events[row]);
			switch (tag) {
				case ColumnType::Name: return event.EventName.c_str();
				case ColumnType::Keyword: return event.KeywordName.c_str();
				case ColumnType::Id: return std::to_wstring(event.Descriptor.Id).c_str();
				case ColumnType::Task: return event.TaskName.c_str();
				case ColumnType::OpCode: return event.OpCodeName.c_str();
				case ColumnType::Level: return event.LevelName.c_str();
				case ColumnType::Source: return StringHelpers::DecodingSourceToString(event.DescodingSource);
				case ColumnType::Guid: return StringHelpers::GuidToString(event.EventGuid).c_str();
				case ColumnType::Message: return event.EventMessage.c_str();
				case ColumnType::Count: return std::to_wstring(event.Properties.size()).c_str();
				case ColumnType::ChannelName: return event.ChannelName.c_str();
			}
			break;
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
	switch (m_CurrentNode) {
		case TreeItemType::Root:
			std::sort(m_Providers.begin(), m_Providers.end(), [&](const auto& a1, const auto& a2) {
				switch (m_ProviderCM.GetColumnTag<ColumnType>(si->SortColumn)) {
					case ColumnType::Name: return SortHelper::Sort(a1->Name(), a2->Name(), asc);
					case ColumnType::Guid: return SortHelper::Sort(a1->GuidAsString(), a2->GuidAsString(), asc);
					case ColumnType::Source: return SortHelper::Sort(a1->SchemaSource(), a2->SchemaSource(), asc);
					case ColumnType::Count: return SortHelper::Sort(a1->EventCount(), a2->EventCount(), asc);
				}
				return false;
				});
			break;

		case TreeItemType::Provider:
			std::sort(m_Events.begin(), m_Events.end(), [&](const auto& ev1, const auto& ev2) {
				auto e1 = m_CurrentProvider->EventInfo(ev1);
				auto e2 = m_CurrentProvider->EventInfo(ev2);
				switch (m_EventsCM.GetColumnTag<ColumnType>(si->SortColumn)) {
					case ColumnType::Name: return SortHelper::Sort(e1.EventName, e2.EventName, asc);
					case ColumnType::Keyword: return SortHelper::Sort(e1.KeywordName, e2.KeywordName, asc);
					case ColumnType::Id: return SortHelper::Sort(e1.Descriptor.Id, e2.Descriptor.Id, asc);
					case ColumnType::OpCode: return SortHelper::Sort(e1.OpCodeName, e2.OpCodeName, asc);
					case ColumnType::Level: return SortHelper::Sort(e1.LevelName, e2.LevelName, asc);
					case ColumnType::Message: return SortHelper::Sort(e1.EventMessage, e2.EventMessage, asc);
					case ColumnType::Source: return SortHelper::Sort(e1.DescodingSource, e2.DescodingSource, asc);
					case ColumnType::Task: return SortHelper::Sort(e1.TaskName, e2.TaskName, asc);
					case ColumnType::Guid: return SortHelper::Sort(StringHelpers::GuidToString(e1.EventGuid), StringHelpers::GuidToString(e2.EventGuid), asc);
					case ColumnType::Count: return SortHelper::Sort(e1.Properties.size(), e2.Properties.size(), asc);
					case ColumnType::ChannelName: return SortHelper::Sort(e1.ChannelName, e2.ChannelName, asc);
				}
				return false;
				});
			break;

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

void CProvidersView::OnTreeSelChanged(HWND tree, HTREEITEM hOld, HTREEITEM hNew) {
	auto newNode = GetItemData<TreeItemType>(m_Tree, hNew);
	auto change = newNode != m_CurrentNode;
	if (change) {
		m_ListViewState[(int)m_CurrentNode] = SaveState(m_List);
		m_CurrentNode = GetItemData<TreeItemType>(m_Tree, hNew);
	}
	m_PropList.SetItemCount(0);
	RefreshList(change);
}

int CProvidersView::GetRowImage(HWND h, int row, int col) const {
	if (h == m_PropList)
		return -1;

	switch (m_CurrentNode) {
		case TreeItemType::Root:
			return m_Providers[row]->SchemaSource() == EtwSchemaSource::Xml ? 1 : 0;

		case TreeItemType::Provider:
			return 2 + m_Events[row].Level;

	}
	return 0;
}

void CProvidersView::OnStateChanged(HWND h, int from, int to, UINT oldState, UINT newState) {
	if (m_CurrentNode == TreeItemType::Root)
		return;

	if (h == m_List && (newState & LVIS_SELECTED) && to == from) {
		//
		// get properties of selected event
		//
		m_Properties = m_CurrentProvider->EventInfo(m_Events[from]).Properties;
		m_PropList.SetItemCount((int)m_Properties.size());
		Sort(m_PropList);
	}
}

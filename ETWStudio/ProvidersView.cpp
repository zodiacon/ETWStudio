// View.cpp : implementation of the CProvidersView class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "ProvidersView.h"
#include <ListViewhelper.h>
#include <SortHelper.h>

BOOL CProvidersView::PreTranslateMessage(MSG* pMsg) {
	pMsg;
	return FALSE;
}

void CProvidersView::InitTree() {
	m_Tree.SetRedraw(FALSE);

	WCHAR computer[MAX_COMPUTERNAME_LENGTH];
	DWORD len = _countof(computer);
	::GetComputerName(computer, &len);
	auto root = InsertTreeItem(m_Tree, computer, TreeIconType::Computer, TreeItemType::Root);
	m_Providers = EtwProvider::EnumProviders();
	for (auto& p : m_Providers) {
		InsertTreeItem(m_Tree, p.Name().c_str(), 
			p.SchemaSource() == EtwSchemaSource::Xml ? TreeIconType::Provider2 : TreeIconType::Provider1, 
			TreeItemType::Provider, root, TVI_SORT);
	}
	m_Tree.SelectItem(root);
	m_Tree.Expand(root, TVE_EXPAND);
	m_Tree.SetRedraw(TRUE);
}

void CProvidersView::RefreshList() {
	if (!LoadState(m_List, m_ListViewState[(int)m_CurrentNode])) {
		//
		// add columns based on view
		//
		auto cm = GetColumnManager(m_List);
		cm->Clear();

		switch (m_CurrentNode) {
			case TreeItemType::Root:
				cm->AddColumn(L"Provider Name", 0, 240, ColumnType::Name);
				cm->AddColumn(L"Provider GUID", 0, 280, ColumnType::Guid);
				cm->AddColumn(L"Source", 0, 60, ColumnType::Type);
				cm->AddColumn(L"Events", LVCFMT_RIGHT, 80, ColumnType::Count);
				break;
		}
		cm->UpdateColumns();
	}

	//
	// build column data
	//
	int count = 0;
	switch (m_CurrentNode) {
		case TreeItemType::Root:
			count = (int)m_Providers.size();
			break;
	}
	m_List.SetItemCount(count);
}

LRESULT CProvidersView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_hWndClient = m_Splitter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN);
	m_List.Create(m_Splitter, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LVS_OWNERDATA | LVS_REPORT);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	m_Tree.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
		TVS_HASLINES | TVS_LINESATROOT | TVS_HASLINES | TVS_HASBUTTONS);

	CImageList images;
	images.Create(16, 16, ILC_COLOR32, 8, 4);
	UINT icons[] = { IDI_COMPUTER, IDI_PROVIDER1, IDI_PROVIDER2 };
	for (auto& icon : icons)
		images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
	m_Tree.SetImageList(images);
	m_List.SetImageList(images, LVSIL_SMALL);

	m_Splitter.SetSplitterPanes(m_Tree, m_List);
	m_Splitter.SetSplitterPosPct(30);

	InitTree();

	return 0;
}

CString CProvidersView::GetColumnText(HWND, int row, int col) const {
	auto cm = GetColumnManager(m_List);

	switch (m_CurrentNode) {
		case TreeItemType::Root:
			auto& item = m_Providers[row];
			switch (cm->GetColumnTag<ColumnType>(col)) {
				case ColumnType::Name: return item.Name().c_str();
				case ColumnType::Guid: return item.GuidAsString().c_str();
				case ColumnType::Type: return item.SchemaSource() == EtwSchemaSource::Mof ? L"MOF" : L"XML";
				case ColumnType::Count: return std::to_wstring(item.EventCount()).c_str();
			}
			break;
	}
	return L"";
}

void CProvidersView::DoSort(const SortInfo* si) {
	if (si == nullptr)
		return;

	switch (m_CurrentNode) {
		case TreeItemType::Root:
			std::sort(m_Providers.begin(), m_Providers.end(), [&](const auto& a1, const auto& a2) {
				switch (si->SortColumn) {
					case 0: return SortHelper::Sort(a1.Name(), a2.Name(), si->SortAscending);
					case 1: return SortHelper::Sort(a1.GuidAsString(), a2.GuidAsString(), si->SortAscending);
					case 2: return SortHelper::Sort(a1.SchemaSource(), a2.SchemaSource(), si->SortAscending);
					case 3: return SortHelper::Sort(a1.EventCount(), a2.EventCount(), si->SortAscending);
				}
				return false;
				});
			break;
	}
}

void CProvidersView::OnTreeSelChanged(HWND tree, HTREEITEM hOld, HTREEITEM hNew) {
	m_ListViewState[(int)m_CurrentNode] = SaveState(m_List);
	m_CurrentNode = GetItemData<TreeItemType>(m_Tree, hNew);
	RefreshList();
}

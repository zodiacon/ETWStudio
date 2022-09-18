// View.cpp : implementation of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "View.h"
#include "SortHelper.h"

BOOL CView::PreTranslateMessage(MSG* pMsg) {
	pMsg;
	return FALSE;
}

CString CView::GetColumnText(HWND, int row, int col) const {
	auto& item = m_Providers[row];
	switch (col) {
		case 0: return item.Name().c_str();
		case 1: return item.GuidAsString().c_str();
		case 2: return item.SchemaSource() == EtwSchemaSource::Mof ? L"MOF" : L"XML";
		case 3: return std::to_wstring(item.EventCount()).c_str();
	}
	return L"";
}

void CView::DoSort(const SortInfo* si) {
	if (si == nullptr)
		return;

	std::sort(m_Providers.begin(), m_Providers.end(), [&](const auto& a1, const auto& a2) {
		switch (si->SortColumn) {
			case 0: return SortHelper::SortStrings(a1.Name(), a2.Name(), si->SortAscending);
			case 1: return SortHelper::SortStrings(a1.GuidAsString(), a2.GuidAsString(), si->SortAscending);
			case 2: return SortHelper::SortNumbers(a1.SchemaSource(), a2.SchemaSource(), si->SortAscending);
			case 3: return SortHelper::SortNumbers(a1.EventCount(), a2.EventCount(), si->SortAscending);
		}
		return false;
		});
}

void CView::Refresh() {
	m_Providers = EtwProvider::EnumProviders();
	m_List.SetItemCountEx((int)m_Providers.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
}

LRESULT CView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_hWndClient = m_List.Create(*this, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_LABELTIP);

	m_List.InsertColumn(0, L"Provider Name", LVCFMT_LEFT, 310);
	m_List.InsertColumn(1, L"Provider GUID", LVCFMT_LEFT, 270);
	m_List.InsertColumn(2, L"Schema Source", LVCFMT_CENTER, 100);
//	m_List.InsertColumn(3, L"Events", LVCFMT_RIGHT, 80);

	Refresh();

	return 0;
}

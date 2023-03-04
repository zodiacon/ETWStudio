#include "pch.h"
#include "resource.h"
#include "ProvidersDlg.h"
#include <SortHelper.h>

void CProvidersDlg::DoSort(SortInfo const* si) {
	auto tag = GetColumnManager(m_List)->GetColumnTag<ColumnType>(si->SortColumn);
	auto asc = si->SortAscending;
	auto compare = [&](const auto& a1, const auto& a2) {
		switch (tag) {
			case ColumnType::Name: return SortHelper::Sort(a1.Name(), a2.Name(), asc);
			case ColumnType::Guid: return SortHelper::Sort(a1.GuidAsString(), a2.GuidAsString(), asc);
			case ColumnType::Source: return SortHelper::Sort(a1.SchemaSource(), a2.SchemaSource(), asc);
			case ColumnType::Count: return SortHelper::Sort(a1.EventCount(), a2.EventCount(), asc);
		}
		return false;
	};
	m_Providers.Sort(compare);
}

LRESULT CProvidersDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	m_List.Attach(GetDlgItem(IDC_LIST));
	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);

	CImageList images;
	images.Create(16, 16, ILC_COLOR32, 8, 4);
	UINT icons[] = {
		IDI_COMPUTER, IDI_PROVIDER1, IDI_PROVIDER2
	};
	for (auto& icon : icons)
		images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
	m_List.SetImageList(images, LVSIL_SMALL);

	m_Edit.SubclassWindow(GetDlgItem(IDC_FILTER));
	m_Edit.SetCueBannerText(L"Type to filter", FALSE);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Provider Name", 0, 240, ColumnType::Name);
	cm->AddColumn(L"Provider GUID", 0, 280, ColumnType::Guid);
	cm->AddColumn(L"Source", 0, 60, ColumnType::Source);

	m_Providers = EtwProvider::EnumProviders();
	m_List.SetItemCount((int)m_Providers.size());

	return 0;
}

LRESULT CProvidersDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	if (wID == IDOK) {
		int index = m_List.GetSelectedIndex();
		if (index < 0) {
			AtlMessageBox(m_hWnd, L"Please select a provider or hit Cancel", IDS_TITLE, MB_ICONWARNING);
			return 0;
		}
		m_SelectedProvider = &m_Providers[index];
	}
	EndDialog(wID);
	return 0;
}

LRESULT CProvidersDlg::OnChangeFilter(WORD, WORD wID, HWND, BOOL&) {
	m_Edit.GetWindowText(m_FilterText);
	m_FilterText.MakeLower();
	if (m_FilterText.IsEmpty())
		m_Providers.Filter(nullptr);
	else {
		m_Providers.Filter([&](auto& item, int index) {
			CString name(item.Name().c_str());
			name.MakeLower();
			if (name.Find(m_FilterText) >= 0)
				return true;
			return false;
			});
		m_List.SetItemCountEx((int)m_Providers.size(), LVSICF_NOSCROLL);
	}
	return 0;
}

CString CProvidersDlg::GetColumnText(HWND h, int row, int col) const {
	auto tag = GetColumnManager(h)->GetColumnTag<ColumnType>(col);
	auto& item = m_Providers[row];
	switch (tag) {
		case ColumnType::Name: return item.Name().c_str();
		case ColumnType::Guid: return item.GuidAsString().c_str();
		case ColumnType::Source: return item.SchemaSource() == EtwSchemaSource::Mof ? L"MOF" : L"XML";
		case ColumnType::Count: return std::to_wstring(item.EventCount()).c_str();
	}
	return L"";
}

int CProvidersDlg::GetRowImage(HWND h, int row, int col) const {
	return m_Providers[row].SchemaSource() == EtwSchemaSource::Xml ? 1 : 0;
}

bool CProvidersDlg::OnDoubleClickList(HWND, int row, int col, POINT const&) {
	SendMessage(WM_COMMAND, IDOK);
	return true;
}

EtwProvider const* CProvidersDlg::GetSelectedProvider() const {
	return m_SelectedProvider;
}


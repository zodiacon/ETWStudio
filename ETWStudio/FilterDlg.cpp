#include "pch.h"
#include "resource.h"
#include "FilterDlg.h"
#include "PropertyFilter.h"
#include <WTLHelper.h>

namespace {
	// same order as CompareType
	const PCWSTR CompareTypes[] = {
		L"Equal",
		L"Not Equal",
		L"Less Than",
		L"Greater Than",
		L"Less Than Or Equal",
		L"Greater Than Or Equal",
		L"Contains",
		L"Does Not Contain",
		L"Starts With",
		L"Ends With",
	};

	PCWSTR ResultToString(FilterResult result) {
		return result == FilterResult::Include ? L"Include" : L"Exclude";
	}

	// light backgrounds, readable with black text in both light and dark mode
	struct HighlightColor {
		PCWSTR Name;
		COLORREF Color;
	};
	const HighlightColor HighlightColors[] = {
		{ L"Red", RGB(255, 190, 190) },
		{ L"Orange", RGB(255, 215, 165) },
		{ L"Yellow", RGB(255, 245, 160) },
		{ L"Green", RGB(190, 240, 190) },
		{ L"Cyan", RGB(180, 235, 245) },
		{ L"Blue", RGB(190, 205, 255) },
		{ L"Purple", RGB(225, 200, 250) },
		{ L"Gray", RGB(215, 215, 215) },
	};

	std::wstring ColorToString(COLORREF color) {
		for (auto& c : HighlightColors)
			if (c.Color == color)
				return c.Name;
		return std::format(L"#{:02X}{:02X}{:02X}", GetRValue(color), GetGValue(color), GetBValue(color));
	}

	const PCWSTR FileHeader = L"ETWStudio Filters 1";
	const PCWSTR HighlightFileHeader = L"ETWStudio Highlights 1";
}

CFilterDlg::CFilterDlg(FilterManager& fm, Mode mode) : m_FilterMgr(fm), m_Mode(mode) {
}

void CFilterDlg::SetApplyToExisting(bool apply) noexcept {
	m_ApplyToExisting = apply;
}

bool CFilterDlg::ApplyToExisting() const noexcept {
	return m_ApplyToExisting;
}

void CFilterDlg::SetDiscardExcluded(bool discard) noexcept {
	m_DiscardExcluded = discard;
}

bool CFilterDlg::DiscardExcluded() const noexcept {
	return m_DiscardExcluded;
}

PropertyFilter* CFilterDlg::GetFilter(int index) const {
	return static_cast<PropertyFilter*>(m_FilterMgr.GetFilter(index));
}

CString CFilterDlg::GetColumnText(HWND, int row, int col) const {
	auto filter = GetFilter(row);
	switch (col) {
		case 0: return filter->GetName().c_str();
		case 1: return CompareTypes[(int)filter->GetCompareType()];
		case 2: return filter->GetText().c_str();
		case 3: return m_Mode == Mode::Highlight ? ColorToString(filter->GetColor()).c_str() : ResultToString(filter->GetResult());
	}
	return CString();
}

ListViewRowCheck CFilterDlg::IsRowChecked(HWND, int row) const {
	return GetFilter(row)->IsEnabled() ? ListViewRowCheck::Checked : ListViewRowCheck::Unchecked;
}

void CFilterDlg::OnListViewClick(HWND, int row, int, POINT const& pt) {
	if (row < 0)
		return;
	LVHITTESTINFO info{};
	info.pt = pt;
	m_List.HitTest(&info);
	if (info.iItem == row && (info.flags & LVHT_ONITEMSTATEICON))
		ToggleFilter(row);
}

bool CFilterDlg::OnDoubleClickList(HWND, int row, int, POINT const&) {
	if (row < 0)
		return false;

	// load into the edit controls so it can be tweaked and added again
	auto filter = GetFilter(row);
	m_PropCB.SetWindowText(filter->GetName().c_str());
	m_CompCB.SetCurSel((int)filter->GetCompareType());
	m_ValueCB.SetWindowText(filter->GetText().c_str());
	m_ActionCB.SelectString(-1, m_Mode == Mode::Highlight ? ColorToString(filter->GetColor()).c_str() : ResultToString(filter->GetResult()));
	return true;
}

void CFilterDlg::OnStateChanged(HWND, int, int, UINT, UINT) {
	UpdateButtons();
}

void CFilterDlg::ToggleFilter(int index) {
	auto filter = GetFilter(index);
	filter->Enable(!filter->IsEnabled());
	m_List.RedrawItems(index, index);
}

void CFilterDlg::RemoveFilter(int index) {
	m_FilterMgr.RemoveFilter(index);
	UpdateList((std::min)(index, (int)m_FilterMgr.GetFilterCount() - 1));
}

void CFilterDlg::Move(int delta) {
	int index = m_List.GetSelectedIndex();
	if (index < 0 || !m_FilterMgr.SwapFilters(index, index + delta))
		return;
	UpdateList(index + delta);
}

void CFilterDlg::UpdateList(int select) {
	m_List.SetItemCountEx((int)m_FilterMgr.GetFilterCount(), LVSICF_NOSCROLL);
	m_List.RedrawItems(0, m_List.GetItemCount());
	if (select >= 0) {
		m_List.SelectItem(select);
		m_List.EnsureVisible(select, FALSE);
	}
	UpdateButtons();
}

void CFilterDlg::UpdateButtons() {
	int index = m_List.GetSelectedIndex();
	int count = (int)m_FilterMgr.GetFilterCount();
	GetDlgItem(IDC_UP).EnableWindow(index > 0);
	GetDlgItem(IDC_DOWN).EnableWindow(index >= 0 && index < count - 1);
	GetDlgItem(IDC_CLEAR).EnableWindow(count > 0);
	GetDlgItem(IDC_SAVE).EnableWindow(count > 0);
}

LRESULT CFilterDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	((CButton)GetDlgItem(IDC_UP)).SetIcon(AtlLoadIconImage(IDI_UP, 0, 24, 24));
	((CButton)GetDlgItem(IDC_DOWN)).SetIcon(AtlLoadIconImage(IDI_DOWN, 0, 24, 24));
	((CButton)GetDlgItem(IDC_SAVE)).SetIcon(AtlLoadIconImage(IDI_SAVE, 0, 24, 24));
	((CButton)GetDlgItem(IDC_LOAD)).SetIcon(AtlLoadIconImage(IDI_OPEN, 0, 24, 24));
	((CButton)GetDlgItem(IDC_CLEAR)).SetIcon(AtlLoadIconImage(IDI_CLEAR, 0, 24, 24));
	((CButton)GetDlgItem(IDC_ADD)).SetIcon(AtlLoadIconImage(IDI_FILTER_ADD, 0, 24, 24));

	bool highlight = m_Mode == Mode::Highlight;
	SetDialogIcon(highlight ? IDI_HIGHLIGHT : IDI_FILTER);
	if (highlight) {
		SetWindowText(L"Highlight");
		GetDlgItem(IDC_APPLYEXISTING).ShowWindow(SW_HIDE);	// highlighting always applies to all rows
		GetDlgItem(IDC_DISCARDEXCLUDED).ShowWindow(SW_HIDE);
	}

	m_List.Attach(GetDlgItem(IDC_LIST));
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_CHECKBOXES);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Property", 0, 120);
	cm->AddColumn(L"Compare", 0, 110);
	cm->AddColumn(L"Value", 0, 140);
	cm->AddColumn(highlight ? L"Color" : L"Action", 0, 60);

	m_PropCB.Attach(GetDlgItem(IDC_PROPERTY));
	for (auto& field : PropertyFilter::GetStandardFields())
		m_PropCB.AddString(field.Name);
	m_PropCB.SelectString(-1, L"Process Name");

	m_CompCB.Attach(GetDlgItem(IDC_COMPARE));
	for (auto& comp : CompareTypes)
		m_CompCB.AddString(comp);
	m_CompCB.SetCurSel((int)CompareType::Equal);

	m_ValueCB.Attach(GetDlgItem(IDC_VALUE));

	m_ActionCB.Attach(GetDlgItem(IDC_ACTION));
	if (highlight) {
		for (auto& c : HighlightColors)
			m_ActionCB.SetItemData(m_ActionCB.AddString(c.Name), c.Color);
		m_ActionCB.SelectString(-1, HighlightColors[0].Name);
	}
	else {
		m_ActionCB.SetItemData(m_ActionCB.AddString(L"Include"), (DWORD_PTR)FilterResult::Include);
		m_ActionCB.SetItemData(m_ActionCB.AddString(L"Exclude"), (DWORD_PTR)FilterResult::Exclude);
		m_ActionCB.SelectString(-1, L"Include");
	}

	CheckDlgButton(IDC_APPLYEXISTING, m_ApplyToExisting ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_DISCARDEXCLUDED, m_DiscardExcluded ? BST_CHECKED : BST_UNCHECKED);

	UpdateList();
	return 0;
}

LRESULT CFilterDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	if (wID == IDOK && m_Mode == Mode::Filter) {
		//
		// with any Include filter, events not explicitly included are dropped
		//
		bool include = false;
		for (auto filter : m_FilterMgr.GetFilters())
			if (filter->IsEnabled() && filter->GetResult() == FilterResult::Include)
				include = true;
		m_FilterMgr.SetDefaultResult(include ? FilterResult::Exclude : FilterResult::Include);
		m_ApplyToExisting = IsDlgButtonChecked(IDC_APPLYEXISTING) == BST_CHECKED;
		m_DiscardExcluded = IsDlgButtonChecked(IDC_DISCARDEXCLUDED) == BST_CHECKED;
	}
	EndDialog(wID);
	return 0;
}

LRESULT CFilterDlg::OnAdd(WORD, WORD, HWND, BOOL&) {
	CString name, text;
	m_PropCB.GetWindowText(name);
	m_ValueCB.GetWindowText(text);
	name.Trim();

	std::wstring error;
	bool highlight = m_Mode == Mode::Highlight;
	auto action = m_ActionCB.GetItemData(m_ActionCB.GetCurSel());
	// a highlight rule "includes" the rows it colors
	auto filter = PropertyFilter::Create((PCWSTR)name, (CompareType)m_CompCB.GetCurSel(), (PCWSTR)text,
		highlight ? FilterResult::Include : (FilterResult)action, error);
	if (!filter) {
		AtlMessageBox(m_hWnd, error.c_str(), IDS_TITLE, MB_ICONWARNING);
		return 0;
	}
	if (highlight)
		filter->SetColor((COLORREF)action);
	if (!text.IsEmpty() && m_ValueCB.FindStringExact(-1, text) < 0)
		m_ValueCB.AddString(text);

	m_FilterMgr.AddFilter(std::move(filter));
	UpdateList((int)m_FilterMgr.GetFilterCount() - 1);
	return 0;
}

LRESULT CFilterDlg::OnMoveUp(WORD, WORD, HWND, BOOL&) {
	Move(-1);
	return 0;
}

LRESULT CFilterDlg::OnMoveDown(WORD, WORD, HWND, BOOL&) {
	Move(1);
	return 0;
}

LRESULT CFilterDlg::OnClear(WORD, WORD, HWND, BOOL&) {
	if (AtlMessageBox(m_hWnd, m_Mode == Mode::Highlight ? L"Remove all highlight rules?" : L"Remove all filters?", IDS_TITLE, MB_ICONQUESTION | MB_OKCANCEL) != IDOK)
		return 0;
	m_FilterMgr.Clear();
	UpdateList();
	return 0;
}

LRESULT CFilterDlg::OnListKeyDown(int, LPNMHDR hdr, BOOL&) {
	auto kd = (NMLVKEYDOWN*)hdr;
	int index = m_List.GetSelectedIndex();
	if (index < 0)
		return 0;
	switch (kd->wVKey) {
		case VK_SPACE: ToggleFilter(index); break;
		case VK_DELETE: RemoveFilter(index); break;
	}
	return 0;
}

LRESULT CFilterDlg::OnListCustomDraw(int, LPNMHDR hdr, BOOL& handled) {
	if (m_Mode != Mode::Highlight) {
		handled = FALSE;
		return 0;
	}
	// a swatch in the Color column
	auto cd = (NMLVCUSTOMDRAW*)hdr;
	switch (cd->nmcd.dwDrawStage) {
		case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW;
		case CDDS_ITEMPREPAINT: return CDRF_NOTIFYSUBITEMDRAW;
		case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
			if (cd->iSubItem == 3 && cd->nmcd.dwItemSpec < m_FilterMgr.GetFilterCount()) {
				// painted here: the dark mode list drawing ignores clrTextBk
				auto color = GetFilter((int)cd->nmcd.dwItemSpec)->GetColor();
				CRect rc;
				m_List.GetSubItemRect((int)cd->nmcd.dwItemSpec, 3, LVIR_BOUNDS, &rc);
				rc.DeflateRect(2, 1);
				CDCHandle dc(cd->nmcd.hdc);
				dc.FillSolidRect(&rc, color);
				dc.SetBkMode(TRANSPARENT);
				dc.SetTextColor(RGB(0, 0, 0));
				rc.left += 4;
				dc.DrawText(ColorToString(color).c_str(), -1, &rc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
				return CDRF_SKIPDEFAULT;
			}
			break;
	}
	return CDRF_DODEFAULT;
}

LRESULT CFilterDlg::OnSave(WORD, WORD, HWND, BOOL&) {
	bool highlight = m_Mode == Mode::Highlight;
	CSimpleFileDialog dlg(FALSE, highlight ? L"etwhighlight" : L"etwfilter", nullptr, OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_ENABLESIZING,
		highlight ? L"Highlight Files (*.etwhighlight)\0*.etwhighlight\0All Files\0*.*\0" : L"Filter Files (*.etwfilter)\0*.etwfilter\0All Files\0*.*\0", m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = IDOK == dlg.DoModal();
	WTLHelper::ResumeHook();
	if (ok && !SaveFilters(dlg.m_szFileName))
		AtlMessageBox(m_hWnd, L"Failed to save filters", IDS_TITLE, MB_ICONERROR);
	return 0;
}

LRESULT CFilterDlg::OnLoad(WORD, WORD, HWND, BOOL&) {
	bool highlight = m_Mode == Mode::Highlight;
	CSimpleFileDialog dlg(TRUE, highlight ? L"etwhighlight" : L"etwfilter", nullptr, OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_ENABLESIZING,
		highlight ? L"Highlight Files (*.etwhighlight)\0*.etwhighlight\0All Files\0*.*\0" : L"Filter Files (*.etwfilter)\0*.etwfilter\0All Files\0*.*\0", m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = IDOK == dlg.DoModal();
	WTLHelper::ResumeHook();
	if (ok && !LoadFilters(dlg.m_szFileName))
		AtlMessageBox(m_hWnd, L"Failed to load filters (invalid file format)", IDS_TITLE, MB_ICONERROR);
	return 0;
}

//
// file format (UTF-8): a header line, then one filter per line:
// enabled <tab> property <tab> compare type <tab> action <tab> value
// (highlights: a different header, and the color as RRGGBB hex instead of the action)
//
bool CFilterDlg::SaveFilters(PCWSTR path) const {
	FILE* fp;
	if (::_wfopen_s(&fp, path, L"w, ccs=UTF-8") != 0)
		return false;
	bool highlight = m_Mode == Mode::Highlight;
	fwprintf(fp, L"%s\n", highlight ? HighlightFileHeader : FileHeader);
	for (size_t i = 0; i < m_FilterMgr.GetFilterCount(); i++) {
		auto filter = GetFilter((int)i);
		auto color = filter->GetColor();
		auto action = highlight ? std::format(L"{:02X}{:02X}{:02X}", GetRValue(color), GetGValue(color), GetBValue(color)) : std::to_wstring((int)filter->GetResult());
		fwprintf(fp, L"%d\t%s\t%d\t%s\t%s\n", filter->IsEnabled() ? 1 : 0, filter->GetName().c_str(),
			(int)filter->GetCompareType(), action.c_str(), filter->GetText().c_str());
	}
	fclose(fp);
	return true;
}

bool CFilterDlg::LoadFilters(PCWSTR path) {
	FILE* fp;
	if (::_wfopen_s(&fp, path, L"r, ccs=UTF-8") != 0)
		return false;

	FilterManager mgr;
	WCHAR line[1024];
	bool highlight = m_Mode == Mode::Highlight;
	bool ok = fgetws(line, _countof(line), fp) && std::wstring_view(line).starts_with(highlight ? HighlightFileHeader : FileHeader);
	while (ok && fgetws(line, _countof(line), fp)) {
		std::wstring text(line);
		while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r'))
			text.pop_back();
		if (text.empty())
			continue;

		std::vector<std::wstring> parts;
		size_t start = 0;
		for (int i = 0; i < 4; i++) {
			auto tab = text.find(L'\t', start);
			if (tab == std::wstring::npos)
				break;
			parts.push_back(text.substr(start, tab - start));
			start = tab + 1;
		}
		if (parts.size() != 4) {
			ok = false;
			break;
		}
		auto compare = _wtoi(parts[2].c_str());
		auto result = highlight ? FilterResult::Include : (FilterResult)_wtoi(parts[3].c_str());
		if (compare < 0 || compare >= _countof(CompareTypes) || (result != FilterResult::Include && result != FilterResult::Exclude)) {
			ok = false;
			break;
		}
		std::wstring error;
		auto filter = PropertyFilter::Create(parts[1], (CompareType)compare, text.substr(start), result, error);
		if (!filter) {
			ok = false;
			break;
		}
		if (highlight) {
			auto rgb = std::wcstoul(parts[3].c_str(), nullptr, 16);
			filter->SetColor(RGB(rgb >> 16, (rgb >> 8) & 0xff, rgb & 0xff));
		}
		filter->Enable(parts[0] == L"1");
		mgr.AddFilter(std::move(filter));
	}
	fclose(fp);
	if (!ok)
		return false;

	m_FilterMgr = std::move(mgr);
	UpdateList(m_FilterMgr.GetFilterCount() ? 0 : -1);
	return true;
}

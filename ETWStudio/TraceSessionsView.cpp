#include "pch.h"
#include "TraceSessionsView.h"
#include "StringHelper.h"
#include <SortHelper.h>

CTraceSessionsView::CTraceSessionsView(IMainFrame* frame) : CFrameView(frame) {
}

CString CTraceSessionsView::GetColumnText(HWND h, int row, int col) const {
	auto& si = m_Sessions[row];
	switch (GetColumnManager(h)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Index: return std::to_wstring(si.Index).c_str();
		case ColumnType::Name: return si.LoggerName.c_str();
		case ColumnType::Guid: return StringHelper::GuidToString(si.Wnode.Guid).c_str();
		case ColumnType::FileName: return si.LogFileName.c_str();
		case ColumnType::TID: return std::to_wstring(HandleToULong(si.LoggerThreadId)).c_str();
		case ColumnType::BufferSize: return std::format("{} KB", si.BufferSize).c_str();
		case ColumnType::MinBuffers: return std::to_wstring(si.MinimumBuffers).c_str();
		case ColumnType::MaxBuffers: return std::to_wstring(si.MaximumBuffers).c_str();
		case ColumnType::LogFileMode: return std::format("0x{:X}", si.LogFileMode).c_str();
		case ColumnType::EnableFlags: return std::format("0x{:X}", si.EnableFlags).c_str();
	}
	return L"";
}

void CTraceSessionsView::DoSort(const SortInfo* si) {
	auto col = GetColumnManager(m_List)->GetColumnTag<ColumnType>(si->SortColumn);

	auto compare = [&](auto& s1, auto& s2) {
		switch (col) {
			case ColumnType::Index: return SortHelper::Sort(s1.Index, s2.Index, si->SortAscending);
			case ColumnType::TID: return SortHelper::Sort(s1.LoggerThreadId, s2.LoggerThreadId, si->SortAscending);
			case ColumnType::Name: return SortHelper::Sort(s1.LoggerName, s2.LoggerName, si->SortAscending);
			case ColumnType::FileName: return SortHelper::Sort(s1.LogFileName, s2.LogFileName, si->SortAscending);
			case ColumnType::Guid: return SortHelper::Sort(StringHelper::GuidToString(s1.Wnode.Guid), StringHelper::GuidToString(s2.Wnode.Guid), si->SortAscending);
			case ColumnType::MinBuffers: return SortHelper::Sort(s1.MinimumBuffers, s2.MinimumBuffers, si->SortAscending);
			case ColumnType::MaxBuffers: return SortHelper::Sort(s1.MaximumBuffers, s2.MaximumBuffers, si->SortAscending);
			case ColumnType::BufferSize: return SortHelper::Sort(s1.BufferSize, s2.BufferSize, si->SortAscending);
			case ColumnType::LogFileMode: return SortHelper::Sort(s1.LogFileMode, s2.LogFileMode, si->SortAscending);
			case ColumnType::EnableFlags: return SortHelper::Sort(s1.EnableFlags, s2.EnableFlags, si->SortAscending);
		}
		return false;
	};
	std::ranges::sort(m_Sessions, compare);

}

int CTraceSessionsView::GetRowImage(HWND, int row, int col) const {
	return m_Sessions[row].Running ? 0 : 1;
}

void CTraceSessionsView::OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState) {
	UpdateUI();
}

void CTraceSessionsView::Refresh() {
	m_Sessions = TraceSessionInfo::EnumTraceSessions();
	Sort(m_List);
	m_List.SetItemCount((int)m_Sessions.size());
}

void CTraceSessionsView::UpdateUI() {
	auto& ui = Frame()->UI();
	auto selected = m_List.GetSelectedIndex();
	ui.UIEnable(ID_SESSION_RUN, selected >= 0 && !m_Sessions[selected].Running);
	ui.UIEnable(ID_SESSION_STOP, selected >= 0 && m_Sessions[selected].Running);
}

LRESULT CTraceSessionsView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_INFOTIP | LVS_EX_LABELTIP);

	CImageList images;
	images.Create(16, 16, ILC_COLOR32, 2, 2);
	UINT icons[] = {
		IDI_RUN, IDI_STOP,
	};
	for (auto& icon : icons)
		images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
	m_List.SetImageList(images, LVSIL_SMALL);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"dummy", 0, 0, 0);
	cm->AddColumn(L"#", LVCFMT_RIGHT, 50, ColumnType::Index);
	cm->AddColumn(L"Name", 0, 300, ColumnType::Name);
	cm->AddColumn(L"GUID", 0, 270, ColumnType::Guid);
	cm->AddColumn(L"Log File", 0, 450, ColumnType::FileName);
	cm->AddColumn(L"Buffer Size", LVCFMT_RIGHT, 80, ColumnType::BufferSize);
	cm->AddColumn(L"Min Buffers", LVCFMT_RIGHT, 80, ColumnType::MinBuffers);
	cm->AddColumn(L"Max Buffers", LVCFMT_RIGHT, 80, ColumnType::MaxBuffers);
	cm->AddColumn(L"Log File Mode", LVCFMT_RIGHT, 90, ColumnType::LogFileMode);
	cm->AddColumn(L"Enable Flags", LVCFMT_RIGHT, 90, ColumnType::EnableFlags);
	cm->DeleteColumn(0);

	Refresh();

	return 0;
}

LRESULT CTraceSessionsView::OnActivate(UINT, WPARAM active, LPARAM, BOOL&) {
	if (active)
		UpdateUI();
	return 0;
}

LRESULT CTraceSessionsView::OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
	m_List.SetFocus();
	return 0;
}

LRESULT CTraceSessionsView::OnDelete(WORD, WORD, HWND, BOOL&) {
	return LRESULT();
}

LRESULT CTraceSessionsView::OnRun(WORD, WORD, HWND, BOOL&) {
	return LRESULT();
}

LRESULT CTraceSessionsView::OnStop(WORD, WORD, HWND, BOOL&) {
	return LRESULT();
}

LRESULT CTraceSessionsView::OnRefresh(WORD, WORD, HWND, BOOL&) {
	Refresh();
	return 0;
}

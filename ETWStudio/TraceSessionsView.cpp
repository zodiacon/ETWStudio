#include "pch.h"
#include "TraceSessionsView.h"
#include "StringHelper.h"

CTraceSessionsView::CTraceSessionsView(IMainFrame* frame) : CFrameView(frame) {
}

CString CTraceSessionsView::GetColumnText(HWND h, int row, int col) const {
	auto& si = m_Sessions[row];
	switch (GetColumnManager(h)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Index: return std::to_wstring(si.Index).c_str();
		case ColumnType::Name: return si.LoggerName.c_str();
		case ColumnType::Guid: return StringHelper::GuidToString(si.Wnode.Guid).c_str();
		case ColumnType::FileName: return si.LogFileName.c_str();

	}
	return L"";
}

int CTraceSessionsView::GetRowImage(HWND, int row, int col) const {
	return 0;
}

void CTraceSessionsView::Refresh() {
	m_Sessions = TraceSessionInfo::EnumTraceSessions();
	m_List.SetItemCount((int)m_Sessions.size());
}

LRESULT CTraceSessionsView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA);
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
	cm->AddColumn(L"GUID", LVCFMT_RIGHT, 90, ColumnType::Guid);
	cm->AddColumn(L"Log File", 0, 250, ColumnType::FileName);
	cm->DeleteColumn(0);

	Refresh();

	return 0;
}

LRESULT CTraceSessionsView::OnActivate(UINT, WPARAM, LPARAM, BOOL&) {
	return LRESULT();
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

#include "pch.h"
#include "resource.h"
#include "LogView.h"
#include <SortHelper.h>
#include <ClipboardHelper.h>
#include "StringHelper.h"

CLogView::CLogView(IMainFrame* frame, std::unique_ptr<TraceSession> session) : CFrameView(frame), m_Session(std::move(session)) {
}

CString CLogView::GetColumnText(HWND h, int row, int col) const {
	auto& evt = *m_Events[row];
	switch (GetColumnManager(h)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::ProcessName: return (evt.GetProcessName()).c_str();
		case ColumnType::PID:
		{
			auto pid = evt.GetProcessId();
			if (pid == 0 || pid == (DWORD)-1)
				return L"";
			return std::format(L"{}", pid).c_str();
		}
		case ColumnType::TID:
		{
			auto tid = evt.GetThreadId();
			if (tid == 0 || tid == (DWORD)-1)
				return L"";
			return std::format(L"{}", tid).c_str();
		}
		case ColumnType::Time: return StringHelper::TimeStampToString(evt.GetTimeStamp()).c_str();
		case ColumnType::Index: return std::format(L"{}", evt.GetIndex()).c_str();
		case ColumnType::EventName: return evt.GetEventStrings().Name.c_str();
		case ColumnType::Task: return evt.GetEventStrings().Task.c_str();
		case ColumnType::Channel: return evt.GetEventStrings().Channel.c_str();
		case ColumnType::Keyword: return evt.GetEventStrings().Keyword.c_str();
		case ColumnType::OpCode: return std::format(L"{} ({})", evt.GetEventStrings().Opcode, evt.GetEventDescriptor().Opcode).c_str();
		case ColumnType::Level: return evt.GetEventStrings().Level.c_str();
		case ColumnType::Message: return evt.GetEventStrings().Message.c_str();
		case ColumnType::Attributes: return evt.GetEventStrings().EventAttributes.c_str();
	}

	return CString();
}

void CLogView::DoSort(const SortInfo* si) {

}

int CLogView::GetRowImage(HWND h, int row, int col) const {
	return m_Events[row]->GetEventDescriptor().Level;
}

void CLogView::OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState) {
}

BOOL CLogView::OnDoubleClickList(HWND h, int row, int col, POINT const& pt) {
	return 0;
}

void CLogView::UpdateUI() {
	auto& ui = Frame()->UI();
	ui.UIEnable(ID_SESSION_RUN, true);
	ui.UIEnable(ID_SESSION_STOP, true);
	ui.UIEnable(ID_SESSION_CLEAR, true);
	ui.UIEnable(ID_VIEW_AUTOSCROLL, true);
	ui.UISetCheck(ID_SESSION_RUN, m_Running);
	ui.UISetCheck(ID_SESSION_STOP, !m_Running);
	ui.UISetCheck(ID_VIEW_AUTOSCROLL, m_AutoScroll);
}

LRESULT CLogView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_INFOTIP | LVS_EX_LABELTIP);

	CImageList images;
	images.Create(16, 16, ILC_COLOR32, 8, 4);
	UINT icons[] = {
		IDI_EVENT, IDI_CRITICAL, IDI_LERROR, IDI_LWARNING, IDI_LINFO, IDI_LDEBUG,
	};
	for (auto& icon : icons)
		images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
	m_List.SetImageList(images, LVSIL_SMALL);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"dummy", 0, 0, 0);
	cm->AddColumn(L"#", LVCFMT_RIGHT, 70, ColumnType::Index);
	cm->AddColumn(L"PID", LVCFMT_RIGHT, 60, ColumnType::PID);
	cm->AddColumn(L"Process Name", 0, 160, ColumnType::ProcessName);
	cm->AddColumn(L"Time", LVCFMT_RIGHT, 90, ColumnType::Time);
	cm->AddColumn(L"TID", LVCFMT_RIGHT, 60, ColumnType::TID);
	cm->AddColumn(L"Level", 0, 80, ColumnType::Level);
	cm->AddColumn(L"Task", 0, 160, ColumnType::Task);
	cm->AddColumn(L"Keyword", 0, 160, ColumnType::Keyword);
	cm->AddColumn(L"Opcode", 0, 100, ColumnType::OpCode);
	cm->AddColumn(L"Name", 0, 100, ColumnType::EventName);
	cm->AddColumn(L"Attributes", 0, 120, ColumnType::Attributes, ColumnFlags::None);
	cm->AddColumn(L"Message", 0, 300, ColumnType::Message);
	cm->DeleteColumn(0);

	return 0;
}

LRESULT CLogView::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	std::lock_guard locker(m_EventsLock);
	if (m_TempEvents.empty()) {
		if (m_Session->IsPaused())
			KillTimer(id);
	}
	else {
		m_Events.append(m_TempEvents.begin(), m_TempEvents.end());
		m_TempEvents.clear();
		m_List.SetItemCountEx((int)m_Events.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
		if (m_AutoScroll)
			m_List.EnsureVisible(m_List.GetItemCount() - 1, FALSE);
	}
	return 0;
}

LRESULT CLogView::OnActivate(UINT, WPARAM active, LPARAM, BOOL&) {
	if (active)
		UpdateUI();
	return 0;
}

LRESULT CLogView::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
	m_Session->Stop();
	return 0;
}

LRESULT CLogView::OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
	m_List.SetFocus();
	return 0;
}

LRESULT CLogView::OnQuickFind(WORD, WORD, HWND, BOOL&) {
	return LRESULT();
}

LRESULT CLogView::OnRun(WORD, WORD, HWND, BOOL&) {
	if (m_Running)
		return 0;

	m_Session->Start([&](auto evt) {
		std::lock_guard locker(m_EventsLock);
		m_TempEvents.push_back(evt);
		});
	SetTimer(1, 1000);
	Frame()->UI().UISetCheck(ID_SESSION_RUN, true);
	Frame()->UI().UISetCheck(ID_SESSION_STOP, false);
	m_Running = true;
	return 0;
}

LRESULT CLogView::OnStop(WORD, WORD, HWND, BOOL&) {
	if (!m_Running)
		return 0;

	m_Session->Pause(true);
	Frame()->UI().UISetCheck(ID_SESSION_RUN, false);
	Frame()->UI().UISetCheck(ID_SESSION_STOP, true);
	m_Running = false;

	return 0;
}

LRESULT CLogView::OnAutoScroll(WORD, WORD, HWND, BOOL&) {
	m_AutoScroll = !m_AutoScroll;
	Frame()->UI().UISetCheck(ID_VIEW_AUTOSCROLL, m_AutoScroll);

	return 0;
}

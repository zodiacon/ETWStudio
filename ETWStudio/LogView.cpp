#include "pch.h"
#include "resource.h"
#include "LogView.h"
#include <SortHelper.h>
#include <ClipboardHelper.h>
#include "StringHelper.h"
#include "FilterDlg.h"
#include "PropertiesDlg.h"
#include "SessionDlg.h"
#include <execution>
#include <fstream>

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
		case ColumnType::Time: return StringHelper::TimeStampToString(evt.GetTimeStamp(), !m_Session->IsRealTimeSession()).c_str();
		case ColumnType::Index: return std::format(L"{}", evt.GetIndex()).c_str();
		case ColumnType::EventName: return evt.GetEventStrings().Name.c_str();
		case ColumnType::Task: return evt.GetEventStrings().Task.c_str();
		case ColumnType::Channel: return evt.GetEventStrings().Channel.c_str();
		case ColumnType::Keyword: return evt.GetEventStrings().Keyword.c_str();
		case ColumnType::OpCode: return std::format(L"{} ({})", evt.GetEventStrings().Opcode, evt.GetEventDescriptor().Opcode).c_str();
		case ColumnType::Level: return evt.GetEventStrings().Level.c_str();
		case ColumnType::Message: return evt.GetEventStrings().Message.c_str();
		case ColumnType::Attributes: return evt.GetEventStrings().EventAttributes.c_str();
		case ColumnType::Properties: return GetProperties(evt).c_str();
	}

	return CString();
}

void CLogView::DoSort(const SortInfo* si) {
	auto col = GetColumnManager(m_List)->GetColumnTag<ColumnType>(si->SortColumn);

	auto compare = [&](auto& e1, auto& e2) {
		switch (col) {
			case ColumnType::Index: return SortHelper::Sort(e1->GetIndex(), e2->GetIndex(), si->SortAscending);
			case ColumnType::PID: return SortHelper::Sort(e1->GetProcessId(), e2->GetProcessId(), si->SortAscending);
			case ColumnType::TID: return SortHelper::Sort(e1->GetThreadId(), e2->GetThreadId(), si->SortAscending);
			case ColumnType::ProcessName: return SortHelper::Sort(e1->GetProcessName(), e2->GetProcessName(), si->SortAscending);
			case ColumnType::Channel: return SortHelper::Sort(e1->GetEventStrings().Channel, e2->GetEventStrings().Channel, si->SortAscending);
			case ColumnType::Task: return SortHelper::Sort(e1->GetEventStrings().Task, e2->GetEventStrings().Task, si->SortAscending);
			case ColumnType::Keyword: return SortHelper::Sort(e1->GetEventStrings().Keyword, e2->GetEventStrings().Keyword, si->SortAscending);
			case ColumnType::Level: return SortHelper::Sort(e1->GetEventStrings().Level, e2->GetEventStrings().Level, si->SortAscending);
			case ColumnType::EventName: return SortHelper::Sort(e1->GetEventStrings().Name, e2->GetEventStrings().Name, si->SortAscending);
			case ColumnType::OpCode: return SortHelper::Sort(e1->GetEventStrings().Opcode, e2->GetEventStrings().Opcode, si->SortAscending);
			case ColumnType::Message: return SortHelper::Sort(e1->GetEventStrings().Message, e2->GetEventStrings().Message, si->SortAscending);
		}
		return false;
	};
	if (m_Events.size() > 100000) {
		CWaitCursor wait;
		std::sort(std::execution::par_unseq, m_Events.begin(), m_Events.end(), compare);
	}
	else {
		std::ranges::sort(m_Events, compare);
	}
}

int CLogView::GetRowImage(HWND h, int row, int col) const {
	return m_Events[row]->GetEventDescriptor().Level;
}

void CLogView::OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState) {
	if ((oldState & LVIS_SELECTED) || (newState & LVIS_SELECTED))
		UpdateUI();
}

BOOL CLogView::OnDoubleClickList(HWND h, int row, int col, POINT const& pt) {
	if (row >= 0) {
		ShowProperties(row);
		return true;
	}
	return 0;
}

BOOL CLogView::OnRightClickList(HWND h, int row, int col, POINT const& pt) {
	if (row >= 0) {
		CMenu menu;
		menu.LoadMenu(IDR_CONTEXT);
		Frame()->DisplayContextMenu(menu.GetSubMenu(1), pt.x, pt.y);
	}
	return 0;
}

bool CLogView::IsSortable(HWND h, int col) const {
	return !m_Running;
}

void CLogView::ShowProperties(int index) const {
	auto dlg = new CPropertiesDlg(m_Events[index], m_List.GetImageList(LVSIL_SMALL).GetIcon(GetRowImage(m_List, index, 0)));
	dlg->Create(m_hWnd);
	dlg->ShowWindow(SW_SHOW);
}

void CLogView::UpdateUI() {
	auto& ui = Frame()->UI();
	ui.UISetCheck(ID_SESSION_RUN, m_Running);
	ui.UISetCheck(ID_SESSION_STOP, !m_Running);
	ui.UISetCheck(ID_VIEW_AUTOSCROLL, m_AutoScroll);
	ui.UIEnable(ID_VIEW_PROPERTIES, m_List.GetSelectedCount() == 1);
	ui.UIEnable(ID_EDIT_COPY, m_List.GetSelectedCount() > 0);
}

bool CLogView::DoSave(PCWSTR path) const {
	std::wofstream stm;
	stm.open(path, std::ios::out);
	if (!stm.good())
		return false;

	if (m_Running)
		m_Session->Pause(true);
	auto str = ListViewHelper::GetAllRowsAsString(m_List, L",", L"\n");
	stm << (PCWSTR)str;
	stm.close();
	if (m_Running)
		m_Session->Pause(false);
	return true;
}

std::wstring CLogView::GetProperties(EventData const& evt) {
	return std::to_wstring(evt.GetProperties().size());

	//std::wstring text;
	//for (auto& prop : evt.GetProperties()) {
	//	text += prop.Name + L": " + evt.FormatProperty(prop) + L";";
	//}
	//return text;
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
	if(m_Session->IsRealTimeSession())
		cm->AddColumn(L"Process Name", 0, 160, ColumnType::ProcessName);
	cm->AddColumn(L"Time", LVCFMT_RIGHT, m_Session->IsRealTimeSession() ? 90 : 150, ColumnType::Time);
	cm->AddColumn(L"TID", LVCFMT_RIGHT, 60, ColumnType::TID);
	cm->AddColumn(L"CPU", LVCFMT_RIGHT, 40, ColumnType::CPU);
	cm->AddColumn(L"Level", 0, 80, ColumnType::Level);
	cm->AddColumn(L"Task", 0, 150, ColumnType::Task);
	cm->AddColumn(L"Keyword", 0, 150, ColumnType::Keyword);
	cm->AddColumn(L"Opcode", 0, 120, ColumnType::OpCode);
	cm->AddColumn(L"Channel", 0, 140, ColumnType::Channel);
	cm->AddColumn(L"Name", 0, 100, ColumnType::EventName);
	cm->AddColumn(L"Attributes", 0, 120, ColumnType::Attributes, ColumnFlags::None);
	cm->AddColumn(L"Message", 0, 250, ColumnType::Message);
	cm->AddColumn(L"Properties", LVCFMT_RIGHT, 60, ColumnType::Properties);
	cm->DeleteColumn(0);

	return 0;
}

LRESULT CLogView::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	std::unique_lock locker(m_EventsLock);
	if (m_TempEvents.empty()) {
		if (!m_Running)
			KillTimer(id);
	}
	else {
		m_Events.insert(m_Events.end(), m_TempEvents.begin(), m_TempEvents.end());
		m_TempEvents.clear();
		locker.unlock();

		m_List.SetItemCountEx((int)m_Events.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
		if (m_Active) {
			if (m_AutoScroll)
				m_List.EnsureVisible(m_List.GetItemCount() - 1, FALSE);
			Frame()->SetStatusText(2, std::format(L"{} Events", m_Events.size()).c_str());
		}
	}
	return 0;
}

LRESULT CLogView::OnActivate(UINT, WPARAM active, LPARAM, BOOL&) {
	m_Active = (bool)active;
	if (active) {
		auto& ui = Frame()->UI();
		ui.UIEnable(ID_SESSION_RUN, true);
		ui.UIEnable(ID_SESSION_STOP, true);
		ui.UIEnable(ID_VIEW_AUTOSCROLL, true);
		ui.UIEnable(ID_EDIT_CLEAR_ALL, true);
		ui.UIEnable(ID_EDIT_FIND, true);
		ui.UIEnable(ID_EDIT_HIGHLIGHT, true);
		ui.UIEnable(ID_EDIT_FILTER, true);
		Frame()->SetStatusIcon(1, AtlLoadIconImage(m_Running ? IDI_RUN : IDI_STOP, 0, 16, 16));
		Frame()->SetStatusText(2, std::format(L"{} Events", m_Events.size()).c_str());

		UpdateUI();
	}
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

	if (m_Session->IsRunning())
		m_Session->Pause(false);
	else {
		auto ok = m_Session->Start([&](auto evt) {
			std::lock_guard locker(m_EventsLock);
			m_TempEvents.push_back(evt);
			});
		if (!ok) {
			AtlMessageBox(m_hWnd, L"Failed to start session", IDR_MAINFRAME, MB_ICONERROR);
			return 0;
		}
	}

	SetTimer(1, 1000);
	Frame()->UI().UISetCheck(ID_SESSION_RUN, true);
	Frame()->UI().UISetCheck(ID_SESSION_STOP, false);
	Frame()->SetStatusIcon(1, AtlLoadIconImage(IDI_RUN, 0, 16, 16));
	m_Running = true;
	return 0;
}

LRESULT CLogView::OnStop(WORD, WORD, HWND, BOOL&) {
	if (!m_Running)
		return 0;

	m_Session->Pause(true);
	Frame()->UI().UISetCheck(ID_SESSION_RUN, false);
	Frame()->UI().UISetCheck(ID_SESSION_STOP, true);
	Frame()->SetStatusIcon(1, AtlLoadIconImage(IDI_STOP, 0, 16, 16));
	m_Running = false;

	return 0;
}

LRESULT CLogView::OnAutoScroll(WORD, WORD, HWND, BOOL&) {
	m_AutoScroll = !m_AutoScroll;
	Frame()->UI().UISetCheck(ID_VIEW_AUTOSCROLL, m_AutoScroll);

	return 0;
}

LRESULT CLogView::OnEditFilter(WORD, WORD, HWND, BOOL&) {
	auto mgr = m_FilterMgr.Clone();
	CFilterDlg dlg(mgr);
	if (IDOK == dlg.DoModal()) {
		m_FilterMgr = std::move(mgr);
	}

	return 0;
}

LRESULT CLogView::OnViewProperties(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_List.GetSelectedCount() == 1);
	ShowProperties(m_List.GetNextItem(-1, LVNI_SELECTED));
	return 0;
}

LRESULT CLogView::OnClearAll(WORD, WORD, HWND, BOOL&) {
	m_Events.clear();
	m_List.SetItemCount(0);

	return 0;
}

LRESULT CLogView::OnFileSave(WORD, WORD, HWND, BOOL&) const {
	ThemeHelper::Suspend();
	CSimpleFileDialog dlg(FALSE, L"log", nullptr, OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_ENABLESIZING,
		L"Log Files (*.log)\0*.txt;*.log\0All Files\0*.*\0", m_hWnd);
	auto ok = IDOK == dlg.DoModal();
	ThemeHelper::Resume();
	if (ok) {
		DoSave(dlg.m_szFileName);
	}
	return 0;
}

LRESULT CLogView::OnCopy(WORD, WORD, HWND, BOOL&) const {
	ClipboardHelper::CopyText(m_hWnd, ListViewHelper::GetSelectedRowsAsString(m_List));
	return 0;
}

LRESULT CLogView::OnEditSession(WORD, WORD, HWND, BOOL&) {
	CSessionDlg dlg(Frame(), *m_Session, true);
	if (IDOK == dlg.DoModal()) {
	}
	return 0;
}

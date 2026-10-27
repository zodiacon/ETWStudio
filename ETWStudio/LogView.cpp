#include "pch.h"
#include "resource.h"
#include "LogView.h"
#include <SortHelper.h>
#include <ClipboardHelper.h>
#include <WTLHelper.h>
#include "StringHelper.h"
#include "FilterDlg.h"
#include "PropertyFilter.h"
#include "PropertiesDlg.h"
#include "SessionDlg.h"
#include "SessionPreset.h"
#include "StackSymbolizer.h"
#include "EventExport.h"
#include <execution>
#include <fstream>

CLogView::CLogView(IMainFrame* frame, std::unique_ptr<TraceSession> session) : CFrameView(frame), m_Session(std::move(session)) {
}

bool CLogView::OpenLogFile(PCWSTR path) {
	m_Session = std::make_unique<TraceSession>();
	m_Running = m_Session->OpenFile(path, [&](EventData* evt) {
		if (!m_Session->IsRunning())
			return;
		std::lock_guard locker(m_EventsLock);
		m_TempEvents.push_back(evt);
	});
	UpdateUI();
	return m_Running;
}

CString CLogView::GetColumnText(HWND h, int row, int col) const {
	return GetEventText(*m_Events[row], GetColumnManager(h)->GetColumnTag<ColumnType>(col));
}

// doesn't touch the list, so an export can call it from its worker thread
CString CLogView::GetEventText(EventData const& evt, ColumnType type) const {
	switch (type) {
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
		case ColumnType::CPU: return std::to_wstring(evt.GetCPU()).c_str();
		case ColumnType::Index: return std::format(L"{}", evt.GetIndex()).c_str();
		case ColumnType::EventName: return evt.GetEventStrings().Name.c_str();
		case ColumnType::Task: return evt.GetEventStrings().Task.c_str();
		case ColumnType::Channel: return evt.GetEventStrings().Channel.c_str();
		case ColumnType::Keyword: return evt.GetEventStrings().Keyword.c_str();
		case ColumnType::OpCode: return std::format(L"{} ({})", evt.GetEventStrings().Opcode, evt.GetEventDescriptor().Opcode).c_str();
		case ColumnType::Level: return StringHelper::LevelToString(evt.GetEventDescriptor().Level);// evt.GetEventStrings().Level.c_str();
		case ColumnType::Message: return evt.GetEventStrings().Message.c_str();
		case ColumnType::Attributes: return evt.GetEventStrings().EventAttributes.c_str();
		case ColumnType::Properties: return GetProperties(evt).c_str();
		case ColumnType::Details: return GetDetails(evt).c_str();
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
		std::sort(std::execution::par, m_Events.begin(), m_Events.end(), compare);
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
	return !m_Running && static_cast<ColumnType>(GetColumnManager(h)->GetColumnTag(col)) != ColumnType::Details;
}

void CLogView::ShowProperties(int index) const {
	auto dlg = new CPropertiesDlg(m_Session.get(), m_Events, index, m_List.GetImageList(LVSIL_SMALL).GetIcon(GetRowImage(m_List, index, 0)));
	dlg->Create(m_hWnd);
	dlg->ShowWindow(SW_SHOW);
	std::erase_if(m_PropertiesDialogs, [](auto h) { return !::IsWindow(h); });
	m_PropertiesDialogs.push_back(dlg->m_hWnd);
}

void CLogView::CloseProperties() {
	for (auto h : m_PropertiesDialogs)
		if (::IsWindow(h))
			::DestroyWindow(h);
	m_PropertiesDialogs.clear();
}

void CLogView::UpdateUI() {
	auto& ui = Frame()->UI();
	ui.UISetCheck(ID_SESSION_RUN, m_Running);
	ui.UISetCheck(ID_SESSION_STOP, !m_Running);
	ui.UISetCheck(ID_VIEW_AUTOSCROLL, m_AutoScroll);
	ui.UIEnable(ID_VIEW_PROPERTIES, m_List.GetSelectedCount() == 1);
	ui.UIEnable(ID_EDIT_COPY, m_List.GetSelectedCount() > 0);
	ui.UIEnable(ID_SESSION_PROPERTIES, !m_Running);
	ui.UIEnable(ID_FILE_SAVE, !m_Running);		// export a stopped session only
	Frame()->SetStatusIcon(1, AtlLoadIconImage(m_Running ? IDI_RUN : IDI_STOP, 0, 16, 16));
}

CProgressDlg::Result CLogView::DoSave(PCWSTR path, SaveFormat format) const {
	auto writer = std::make_unique<Utf8Writer>(path);
	if (!writer->IsOpen())
		return CProgressDlg::Result::Failed;

	// the CSV columns: the visible ones, in display order, as displayed
	std::vector<std::pair<std::wstring, ColumnType>> columns;
	if (format == SaveFormat::Csv) {
		auto cm = GetColumnManager(m_List);
		int count = m_List.GetHeader().GetItemCount();
		std::vector<int> order(count);
		m_List.GetColumnOrderArray(count, order.data());
		for (auto col : order) {
			WCHAR name[128]{};
			LVCOLUMN lvc{ LVCF_TEXT };
			lvc.pszText = name;
			lvc.cchTextMax = _countof(name);
			m_List.GetColumn(col, &lvc);
			columns.push_back({ name, cm->GetColumnTag<ColumnType>(col) });
		}
	}

	// the dialog is modal, so the events stay as they are meanwhile
	CProgressDlg dlg(L"Export", L"Exported", m_Events.size(), [&](auto const& report) {
		bool ok = format == SaveFormat::Csv ? SaveCsv(*writer, columns, report) : EventExport::WriteJson(*writer, m_Events, report);
		return ok && writer->Flush();
	});
	auto result = dlg.Run(m_hWnd);
	if (result != CProgressDlg::Result::Done) {
		writer.reset();		// closes the file
		::DeleteFile(path);
	}
	return result;
}

bool CLogView::SaveCsv(Utf8Writer& writer, std::span<std::pair<std::wstring, ColumnType> const> columns,
	std::function<bool(size_t)> const& progress) const {
	writer << L"\xFEFF";	// BOM, so Excel detects UTF-8

	for (size_t i = 0; i < columns.size(); i++)
		writer << (i ? L"," : L"") << EventExport::CsvField(columns[i].first);
	writer << L"\r\n";

	for (size_t row = 0; row < m_Events.size(); row++) {
		if (row % 256 == 0 && !progress(row))
			return false;
		for (size_t i = 0; i < columns.size(); i++)
			writer << (i ? L"," : L"") << EventExport::CsvField((PCWSTR)GetEventText(*m_Events[row], columns[i].second));
		writer << L"\r\n";
	}
	return writer.Ok();
}

std::wstring CLogView::GetProperties(EventData const& evt) {
	return std::to_wstring(evt.GetProperties().size());
}

std::wstring CLogView::GetDetails(EventData const& evt) {
	if (evt.UserText.empty()) {
		std::wstring text;
		for (auto& prop : evt.GetProperties() | std::views::take(8)) {
			text += prop.Name + L": " + evt.FormatProperty(prop) + L"; ";
		}
		evt.UserText = std::move(text);
	}
	return evt.UserText;
}

LRESULT CLogView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	// clip siblings: the quick find box sits on top of the list
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | LVS_REPORT | LVS_OWNERDATA);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_INFOTIP | LVS_EX_LABELTIP);

	CImageList images;
	images.Create(16, 16, ILC_COLOR32, 8, 4);
	UINT icons[] = {
		IDI_EVENT, IDI_CRITICAL, IDI_LERROR, IDI_LWARNING, IDI_LINFO, IDI_LDEBUG,
	};
	for (auto& icon : icons)
		images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
	m_List.SetImageList(images, LVSIL_SMALL);

	auto realtimeSession = m_Session && m_Session->IsRealTimeSession();
	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"dummy", 0, 0, 0);
	cm->AddColumn(L"#", LVCFMT_RIGHT, 70, ColumnType::Index);
	cm->AddColumn(L"PID", LVCFMT_RIGHT, 60, ColumnType::PID);
	if(realtimeSession)
		cm->AddColumn(L"Process Name", 0, 160, ColumnType::ProcessName);
	cm->AddColumn(L"Time", LVCFMT_RIGHT, realtimeSession ? 90 : 150, ColumnType::Time);
	cm->AddColumn(L"TID", LVCFMT_RIGHT | LVCFMT_FIXED_WIDTH, 60, ColumnType::TID);
	cm->AddColumn(L"CPU", LVCFMT_RIGHT | LVCFMT_FIXED_WIDTH, 40, ColumnType::CPU);
	cm->AddColumn(L"Level", 0, 80, ColumnType::Level);
	cm->AddColumn(L"Task", 0, 150, ColumnType::Task);
	cm->AddColumn(L"Keyword", 0, 150, ColumnType::Keyword);
	cm->AddColumn(L"Opcode", 0, 120, ColumnType::OpCode);
	cm->AddColumn(L"Channel", 0, 140, ColumnType::Channel);
	cm->AddColumn(L"Name", 0, 100, ColumnType::EventName);
	cm->AddColumn(L"Attributes", 0, 120, ColumnType::Attributes, ColumnFlags::None);
	cm->AddColumn(L"Message", 0, 200, ColumnType::Message);
	cm->AddColumn(L"Props", LVCFMT_RIGHT | LVCFMT_FIXED_WIDTH, 50, ColumnType::Properties);
	cm->AddColumn(L"Details", 0, 400, ColumnType::Details);
	cm->DeleteColumn(0);

	if (auto hFont = Frame()->GetFont())
		m_List.SetFont(hFont);

	CEdit edit;
	edit.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 0, QuickFindId);
	m_QuickFind.SubclassWindow(edit);
	m_QuickFind.SetFont(m_List.GetFont());
	m_QuickFind.SetWatermark(L"Find (Enter: next, Shift+Enter: previous)");

	SetTimer(1, 1400, nullptr);

	return 0;
}

LRESULT CLogView::OnUpdateFont(UINT, WPARAM wParam, LPARAM, BOOL&) {
	// a null font reverts the list to the default font
	m_List.SetFont((HFONT)wParam);
	m_QuickFind.SetFont(m_List.GetFont());
	PositionQuickFind();
	return 0;
}

LRESULT CLogView::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	if (m_Done) {
		KillTimer(1);
		return 0;
	}
	if (!m_Done && m_Session->IsDone()) {
		m_Done = true;
		AddPendingEvents();		// the last ones (all of them, for a file processed before the first tick)
		LRESULT result;
		Frame()->SetStatusText(3, L"Log file processing complete");
		return ProcessWindowMessage(m_hWnd, WM_COMMAND, ID_SESSION_STOP, 0, result, 1);
	}
	AddPendingEvents();
	return 0;
}

void CLogView::AddPendingEvents() {
	{
		std::lock_guard locker(m_EventsLock);
		if (m_TempEvents.empty())
			return;
		m_Events.insert(m_Events.end(), m_TempEvents.begin(), m_TempEvents.end());
		m_TempEvents.clear();
	}

	m_List.SetItemCountEx((int)m_Events.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	if (m_Active) {
		if (m_AutoScroll)
			m_List.EnsureVisible(m_List.GetItemCount() - 1, FALSE);
		Frame()->SetStatusText(2, std::format(L"{} Events", m_Events.size()).c_str());
	}
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
	KillTimer(1);
	CloseProperties();
	m_Session->Stop(true);
	StackSymbolizer::Get().Forget(m_Session.get());
	m_Events.clear();

	return 0;
}

LRESULT CLogView::OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
	m_List.SetFocus();
	return 0;
}

LRESULT CLogView::OnQuickFind(WORD, WORD, HWND, BOOL&) {
	if (m_QuickFind.IsWindowVisible() && ::GetFocus() == m_QuickFind) {
		m_QuickFind.ShowWindow(SW_HIDE);
		m_List.SetFocus();
	}
	else {
		PositionQuickFind();
		m_QuickFind.SetWindowPos(HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
		m_QuickFind.SetFocus();
	}
	return 0;
}

LRESULT CLogView::OnQuickFindNotify(WORD code, WORD, HWND, BOOL&) {
	switch (code) {
		case EN_DELAYCHANGE: QuickFind(true, true); break;		// typing: the current row may match the longer text
		case CQuickFindBox::FindNext: QuickFind(true, false); break;
		case CQuickFindBox::FindPrevious: QuickFind(false, false); break;
		case CQuickFindBox::Close:
			m_QuickFind.ShowWindow(SW_HIDE);
			m_List.SetFocus();
			break;
	}
	return 0;
}

void CLogView::PositionQuickFind() {
	if (!m_QuickFind)
		return;
	CRect rc, header;
	GetClientRect(&rc);
	m_List.GetHeader().GetWindowRect(&header);
	CClientDC dc(m_QuickFind);
	dc.SelectFont(m_QuickFind.GetFont());
	TEXTMETRIC tm;
	dc.GetTextMetrics(&tm);
	int width = tm.tmAveCharWidth * 40, height = tm.tmHeight + 8;
	m_QuickFind.MoveWindow(rc.right - width - ::GetSystemMetrics(SM_CXVSCROLL) - 8, header.Height() + 6, width, height);
}

LRESULT CLogView::OnSize(UINT, WPARAM, LPARAM, BOOL& handled) {
	handled = FALSE;	// the frame view sizes the list
	if (m_QuickFind && m_QuickFind.IsWindowVisible())
		PositionQuickFind();
	return 0;
}

namespace {
	bool ContainsNoCase(PCWSTR text, std::wstring const& lowerNeedle) {
		std::wstring_view view(text);
		return std::search(view.begin(), view.end(), lowerNeedle.begin(), lowerNeedle.end(),
			[](wchar_t a, wchar_t b) { return ::towlower(a) == b; }) != view.end();
	}
}

void CLogView::QuickFind(bool forward, bool includeCurrent) {
	CString text;
	m_QuickFind.GetWindowText(text);
	int count = (int)m_Events.size();
	if (text.IsEmpty() || count == 0) {
		Frame()->SetStatusText(3, L"");
		return;
	}
	std::wstring needle((PCWSTR)text.MakeLower());

	int current = m_List.GetNextItem(-1, LVNI_FOCUSED);
	int start;
	if (current < 0)
		start = forward ? 0 : count - 1;
	else
		start = includeCurrent ? current : current + (forward ? 1 : -1);

	CWaitCursor wait;
	int columns = m_List.GetHeader().GetItemCount();
	for (int n = 0; n < count; n++) {
		int row = ((start + (forward ? n : -n)) % count + count) % count;
		for (int col = 0; col < columns; col++) {
			if (ContainsNoCase(GetColumnText(m_List, row, col), needle)) {
				m_List.SetItemState(-1, 0, LVIS_SELECTED);
				m_List.SelectItem(row);
				Frame()->SetStatusText(3, std::format(L"Found in event #{}", m_Events[row]->GetIndex()).c_str());
				return;
			}
		}
	}
	::MessageBeep(MB_ICONWARNING);
	Frame()->SetStatusText(3, std::format(L"\"{}\" not found", (PCWSTR)text).c_str());
}

COLORREF CLogView::GetHighlightColor(EventData* evt) const {
	for (auto filter : m_Highlights.GetFilters()) {
		if (filter->IsEnabled() && filter->Eval(evt) == FilterResult::Include)
			return static_cast<PropertyFilter*>(filter)->GetColor();
	}
	return CLR_INVALID;
}

LRESULT CLogView::OnListCustomDraw(int, LPNMHDR hdr, BOOL& handled) {
	if (hdr->hwndFrom != m_List || m_Highlights.GetFilterCount() == 0) {
		handled = FALSE;
		return 0;
	}
	auto cd = (NMLVCUSTOMDRAW*)hdr;
	switch (cd->nmcd.dwDrawStage) {
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;

		case CDDS_ITEMPREPAINT:
			if (cd->nmcd.dwItemSpec < m_Events.size()) {
				if (auto color = GetHighlightColor(m_Events[cd->nmcd.dwItemSpec]); color != CLR_INVALID) {
					cd->clrTextBk = color;
					cd->clrText = RGB(0, 0, 0);
					return CDRF_NEWFONT;
				}
			}
			break;
	}
	return CDRF_DODEFAULT;
}

LRESULT CLogView::OnEditHighlight(WORD, WORD, HWND, BOOL&) {
	auto rules = m_Highlights.Clone();
	CFilterDlg dlg(rules, CFilterDlg::Mode::Highlight);
	if (IDOK == dlg.DoModal()) {
		m_Highlights = std::move(rules);
		m_List.Invalidate();
	}
	return 0;
}

LRESULT CLogView::OnRun(WORD, WORD, HWND, BOOL&) {
	if (m_Running || m_Done)	// a fully processed log file has nothing left to run
		return 0;

	if (m_Session->IsRunning())
		m_Session->Pause(false);
	else {
		// a stopped log file continues where it stopped
		auto ok = m_Session->Start([&](EventData* evt) {
			std::lock_guard locker(m_EventsLock);
			m_TempEvents.push_back(evt);
			}, !m_Session->IsRealTimeSession());
		if (!ok) {
			AtlMessageBox(m_hWnd, L"Failed to start session", IDR_MAINFRAME, MB_ICONERROR);
			return 0;
		}
	}

	m_Running = true;
	UpdateUI();
	return 0;
}

LRESULT CLogView::OnStop(WORD, WORD, HWND, BOOL&) {
	if (!m_Running)
		return 0;

	m_Session->Pause(true);
	m_Running = false;
	UpdateUI();

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
	dlg.SetApplyToExisting(m_ApplyFilterToExisting);
	if (IDOK == dlg.DoModal()) {
		m_FilterMgr = std::move(mgr);
		m_Session->SetFilterManager(m_FilterMgr.Clone());
		m_ApplyFilterToExisting = dlg.ApplyToExisting();
		if (m_ApplyFilterToExisting)
			ApplyFilterToExisting();
	}

	return 0;
}

void CLogView::ApplyFilterToExisting() {
	CloseProperties();		// they index into m_Events

	// pending events are part of the snapshot, so drop them to avoid duplicates
	auto events = m_Session->SnapshotEvents([&] {
		std::lock_guard locker(m_EventsLock);
		m_TempEvents.clear();
		});

	// events excluded while capturing were never stored, so loosening a filter can't bring them back
	std::vector<char> keep(events.size());
	std::for_each(std::execution::par, events.begin(), events.end(), [&](auto& evt) {
		keep[&evt - events.data()] = m_FilterMgr.Eval(evt) != FilterResult::Exclude;
		});
	m_Events.clear();
	for (size_t i = 0; i < events.size(); i++)
		if (keep[i])
			m_Events.push_back(events[i]);

	ClearSort(m_List);
	m_List.SetItemCountEx((int)m_Events.size(), LVSICF_NOSCROLL);
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
	Frame()->SetStatusText(2, std::format(L"{} Events", m_Events.size()).c_str());
	UpdateUI();
}

LRESULT CLogView::OnViewProperties(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_List.GetSelectedCount() == 1);
	ShowProperties(m_List.GetNextItem(-1, LVNI_SELECTED));
	return 0;
}

LRESULT CLogView::OnClearAll(WORD, WORD, HWND, BOOL&) {
	CloseProperties();
	m_Session->ClearEvents([&] {
		std::lock_guard locker(m_EventsLock);
		m_TempEvents.clear();
		m_Events.clear();
		});
	m_List.SetItemCount(0);
	Frame()->SetStatusText(2, L"0 Events");
	UpdateUI();

	return 0;
}

LRESULT CLogView::OnFileSave(WORD, WORD, HWND, BOOL&) {
	// the menu item is disabled while running, but the accelerator still gets here
	if (m_Running) {
		AtlMessageBox(m_hWnd, L"Stop the session before saving", IDS_TITLE, MB_ICONINFORMATION);
		return 0;
	}
	AddPendingEvents();		// everything received before stopping

	WTLHelper::SuspendHook();
	CSimpleFileDialog dlg(FALSE, L"csv", nullptr, OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_ENABLESIZING,
		L"CSV Files - displayed columns (*.csv)\0*.csv\0JSON Files - all event data (*.json)\0*.json\0", m_hWnd);
	auto ok = IDOK == dlg.DoModal();
	WTLHelper::ResumeHook();
	if (ok) {
		// the extension decides; otherwise the selected file type
		auto dot = ::wcsrchr(dlg.m_szFileName, L'.');
		auto ext = dot ? dot : L"";
		auto format = ::_wcsicmp(ext, L".json") == 0 ? SaveFormat::Json :
			::_wcsicmp(ext, L".csv") == 0 ? SaveFormat::Csv : dlg.m_ofn.nFilterIndex == 2 ? SaveFormat::Json : SaveFormat::Csv;
		switch (DoSave(dlg.m_szFileName, format)) {
			case CProgressDlg::Result::Failed:
				AtlMessageBox(m_hWnd, L"Failed to save the file", IDS_TITLE, MB_ICONERROR);
				break;
			case CProgressDlg::Result::Canceled:
				Frame()->SetStatusText(3, L"Export canceled");
				break;
			default:
				Frame()->SetStatusText(3, std::format(L"Saved {} events to {}", m_Events.size(), dlg.m_szFileName).c_str());
				break;
		}
	}
	return 0;
}

LRESULT CLogView::OnSaveSession(WORD, WORD, HWND, BOOL&) const {
	if (!m_Session->IsRealTimeSession()) {
		AtlMessageBox(m_hWnd, L"Only live trace sessions can be saved as session files", IDS_TITLE, MB_ICONINFORMATION);
		return 0;
	}
	auto preset = SessionPreset::FromSession(*m_Session);
	WTLHelper::SuspendHook();
	CSimpleFileDialog dlg(FALSE, L"etwsession", preset.Name.c_str(), OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_ENABLESIZING,
		L"ETW Studio Sessions (*.etwsession)\0*.etwsession\0All Files\0*.*\0", m_hWnd);
	auto ok = IDOK == dlg.DoModal();
	WTLHelper::ResumeHook();
	if (ok && !preset.Save(dlg.m_szFileName))
		AtlMessageBox(m_hWnd, L"Failed to save the session file", IDS_TITLE, MB_ICONERROR);
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

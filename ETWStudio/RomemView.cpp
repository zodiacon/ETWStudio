#include "pch.h"
#include "resource.h"
#include "RomemView.h"
#include <SortHelper.h>
#include <ClipboardHelper.h>
#include "StringHelper.h"
#include "FilterDlg.h"
#include "PropertiesDlg.h"
#include <execution>
#include <fstream>
#include <DbgHelp.h>

#pragma comment(lib, "dbghelp")

CRomemView::CRomemView(IMainFrame* frame, std::unique_ptr<TraceSession> session) : CFrameView(frame), m_Session(std::move(session)) {
}

CString CRomemView::GetColumnText(HWND h, int row, int col) const {
	if (h == m_AllList) {
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
			case ColumnType::CPU: return std::to_wstring(evt.GetCPU()).c_str();
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
	}
	else if (h == m_SummaryList) {
		enum class ItemType {
			PID, EXEPath, CreateTime,
			TotalEvents,
			Allocations, Frees, Reallocations, AllocsDiff,
			malloc, free, realloc, mallocDiff,
			RtlAllocateHeap, RtlFreeHeap, RtlReAllocateHeap, RtlAllocDiff,
			UniqueStacks,
		};

		if (col == 0) {
			static PCWSTR names[] = {
				L"Process ID", L"EXE Path", L"Create Time",
				L"Total Events",
				L"Allocations", L"Frees", L"Reallocations", L"Allocs - Frees",
				L"malloc", L"free", L"realloc", L"malloc - free",
				L"RtlAllocateHeap", L"RtlFreeHeap", L"RtlReAllocateHeap", L"RtlAlloc - Free",
				L"Unique Stacks",
			};
			return row < _countof(names) ? names[row] : L"";
		}
		ATLASSERT(col == 1);
		switch (static_cast<ItemType>(row)) {
			case ItemType::PID: return std::to_wstring(m_FirstEvent ? m_FirstEvent->GetProcessId() : 0).c_str();
			case ItemType::EXEPath: return m_FirstEvent ? m_FirstEvent->GetProperty(L"ExePath")->GetUnicodeString() : L"";
			case ItemType::CreateTime: return m_FirstEvent ? StringHelper::TimeStampToString(m_FirstEvent->GetProperty(L"CreateTime")->GetValue<ULONGLONG>(), true).c_str() : L"";
			case ItemType::TotalEvents: return std::to_wstring(m_Events.size()).c_str();
			case ItemType::Allocations: return std::to_wstring(m_Totals[Event_Op_Alloc]).c_str();
			case ItemType::Frees: return std::to_wstring(m_Totals[Event_Op_Free]).c_str();
			case ItemType::Reallocations: return std::to_wstring(m_Totals[Event_Op_Realloc]).c_str();
			case ItemType::AllocsDiff: return std::to_wstring(m_Totals[Event_Op_Alloc] - m_Totals[Event_Op_Realloc] - m_Totals[Event_Op_Free]).c_str();
			case ItemType::malloc: return std::to_wstring(m_Totals[Event_malloc]).c_str();
			case ItemType::free: return std::to_wstring(m_Totals[Event_free]).c_str();
			case ItemType::realloc: return std::to_wstring(m_Totals[Event_realloc]).c_str();
			case ItemType::mallocDiff: return std::to_wstring(m_Totals[Event_malloc] - m_Totals[Event_realloc] - m_Totals[Event_free]).c_str();
			case ItemType::RtlAllocateHeap: return std::to_wstring(m_Totals[Event_RtlAllocateHeap]).c_str();
			case ItemType::RtlFreeHeap: return std::to_wstring(m_Totals[Event_RtlFreeHeap]).c_str();
			case ItemType::RtlReAllocateHeap: return std::to_wstring(m_Totals[Event_RtlReAllocateHeap]).c_str();
			case ItemType::RtlAllocDiff: return std::to_wstring(m_Totals[Event_RtlAllocateHeap] - m_Totals[Event_RtlReAllocateHeap] - m_Totals[Event_RtlFreeHeap]).c_str();
			case ItemType::UniqueStacks: return std::to_wstring(m_CallsByStack.size()).c_str();
		}
	}
	else if (h == m_ByStackList) {
		auto& cs = m_CallStacks[row];
		switch (GetColumnManager(h)->GetColumnTag<ColumnType>(col)) {
			case ColumnType::Hash: return std::format(L"0x{:X}", cs->Hash).c_str();
			case ColumnType::Count: return std::to_wstring(cs->Count).c_str();
			case ColumnType::Frames: return std::to_wstring(cs->Frames.size()).c_str();
		}

	}
	else if (h == m_CallStackList) {
		auto& cs = m_CallStacks[m_ByStackList.GetSelectedIndex()];
		auto& frame = cs->Frames[row];
		switch (GetColumnManager(h)->GetColumnTag<ColumnType>(col)) {
			case ColumnType::Index: return std::to_wstring(frame.Index).c_str();
			case ColumnType::Address: return std::format(L"0x{:X}", reinterpret_cast<size_t>(frame.Address)).c_str();
			case ColumnType::Symbol: return frame.Symbol.IsEmpty() ? (frame.Symbol = GetSymbol(frame.Address)) : frame.Symbol;
			case ColumnType::Source: return frame.Source.IsEmpty() ? (frame.Source = GetSymbolSource(frame.Address)) : frame.Source;
		}
	}
	return CString();
}

void CRomemView::DoSort(const SortInfo* si) {
	if (si->hWnd == m_AllList) {
		auto col = GetColumnManager(si->hWnd)->GetColumnTag<ColumnType>(si->SortColumn);

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
	else if (si->hWnd == m_ByStackList) {
		auto col = GetColumnManager(si->hWnd)->GetColumnTag<ColumnType>(si->SortColumn);

		auto compare = [&](auto& e1, auto& e2) {
			switch (col) {
				case ColumnType::Hash: return SortHelper::Sort(e1->Hash, e2->Hash, si->SortAscending);
				case ColumnType::Count: return SortHelper::Sort(e1->Count, e2->Count, si->SortAscending);
				case ColumnType::Frames: return SortHelper::Sort(e1->Frames.size(), e2->Frames.size(), si->SortAscending);
			}
			return false;
		};
		std::ranges::sort(m_CallStacks, compare);
	}
}

int CRomemView::GetRowImage(HWND h, int row, int col) const {
	if (h == m_AllList)
		return m_Events[row]->GetEventDescriptor().Level;
	return 0;
}

void CRomemView::OnStateChanged(HWND h, int from, int to, UINT oldState, UINT newState) {
	if ((oldState & LVIS_SELECTED) || (newState & LVIS_SELECTED)) {
		if (h == m_ByStackList) {
			auto sel = m_ByStackList.GetSelectedIndex();
			m_CallStackList.SetItemCountEx(sel < 0 ? 0 : (int)m_CallStacks[sel]->Frames.size(), LVSICF_NOSCROLL);
		}
		UpdateUI();
	}
}

BOOL CRomemView::OnDoubleClickList(HWND h, int row, int col, POINT const& pt) {
	if (row >= 0) {
		ShowProperties(row);
		return true;
	}
	return 0;
}

BOOL CRomemView::OnRightClickList(HWND h, int row, int col, POINT const& pt) {
	if (row >= 0) {
		CMenu menu;
		menu.LoadMenu(IDR_CONTEXT);
		Frame()->DisplayContextMenu(menu.GetSubMenu(1), pt.x, pt.y);
	}
	return 0;
}

bool CRomemView::IsSortable(HWND h, int col) const {
	if (m_Tabs.GetPageHWND(m_Tabs.GetActivePage()) != m_AllList)
		return true;

	return !m_Running;
}

void CRomemView::ShowProperties(int index) const {
	auto dlg = new CPropertiesDlg(m_Events[index], m_AllList.GetImageList(LVSIL_SMALL).GetIcon(GetRowImage(m_AllList, index, 0)));
	dlg->Create(m_hWnd);
	dlg->ShowWindow(SW_SHOW);
}

void CRomemView::UpdateUI() {
	auto& ui = Frame()->UI();
	ui.UISetCheck(ID_SESSION_RUN, m_Running);
	ui.UISetCheck(ID_SESSION_STOP, !m_Running);
	ui.UISetCheck(ID_VIEW_AUTOSCROLL, m_AutoScroll);
	if (m_AllList) {
		ui.UIEnable(ID_VIEW_PROPERTIES, m_AllList.GetSelectedCount() == 1);
		ui.UIEnable(ID_EDIT_COPY, m_AllList.GetSelectedCount() > 0);
	}
}

bool CRomemView::DoSave(PCWSTR path) const {
	std::wofstream stm;
	stm.open(path, std::ios::out);
	if (!stm.good())
		return false;

	if (m_Running)
		m_Session->Pause(true);
	auto str = ListViewHelper::GetAllRowsAsString(m_AllList, L",", L"\n");
	stm << (PCWSTR)str;
	stm.close();
	if (m_Running)
		m_Session->Pause(false);
	return true;
}

std::wstring CRomemView::GetProperties(EventData const& evt) {
	return std::to_wstring(evt.GetProperties().size());

	//std::wstring text;
	//for (auto& prop : evt.GetProperties()) {
	//	text += prop.Name + L": " + evt.FormatProperty(prop) + L";";
	//}
	//return text;
}

CString CRomemView::GetSymbol(PVOID address) const {
	ATLASSERT(m_hProcess);
	auto const size = sizeof(SYMBOL_INFO) + 1024;
	auto static buffer = std::make_unique<BYTE[]>(size);
	auto static si = reinterpret_cast<SYMBOL_INFO*>(buffer.get());
	si->MaxNameLen = 1024;
	si->SizeOfStruct = sizeof(*si);
	DWORD64 disp = 0;
	if (::SymFromAddr(m_hProcess, reinterpret_cast<DWORD64>(address), &disp, si)) {
		static IMAGEHLP_MODULEW64 module{ sizeof(module) };
		::SymGetModuleInfoW64(m_hProcess, reinterpret_cast<DWORD64>(address), &module);
		CString sym = CString(module.ModuleName) + L"!" + si->Name;
		if(disp)
			sym += CString(L"+") + std::to_wstring(disp).c_str();
		return sym;
	}
	return L"<Unknown>";
}

CString CRomemView::GetSymbolSource(PVOID address) const {
	static IMAGEHLP_LINEW64 line{ sizeof(line) };
	DWORD disp = 0;
	if (::SymGetLineFromAddrW64(m_hProcess, reinterpret_cast<DWORD64>(address), &disp, &line)) {
		return std::format(L"{} (Line: {})", line.FileName, line.LineNumber).c_str();
	}

	return CString();
}

LRESULT CRomemView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_MonoFont.CreatePointFont(110, L"Consolas");
	m_Tabs.m_bTabCloseButton = false;
	m_hWndClient = m_Tabs.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN);

	CImageList images;
	images.Create(16, 16, ILC_COLOR32, 8, 4);
	UINT icons[] = {
		IDI_EVENT, IDI_CRITICAL, IDI_LERROR, IDI_LWARNING, IDI_LINFO, IDI_LDEBUG,
	};
	for (auto& icon : icons)
		images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
	m_Tabs.SetImageList(images);

	m_SummaryList.Create(m_Tabs, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA);
	m_SummaryList.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_LABELTIP);
	m_SummaryList.SetImageList(images, LVSIL_SMALL);
	m_Tabs.AddPage(m_SummaryList, L"Summary", 0);

	m_StackSplitter.Create(m_Tabs, rcDefault, nullptr, WS_VISIBLE | WS_CHILD | WS_CLIPCHILDREN);
	m_ByStackList.Create(m_StackSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL);
	m_ByStackList.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_LABELTIP);
	m_ByStackList.SetImageList(images, LVSIL_SMALL);
	m_CallStackList.Create(m_StackSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL);
	m_CallStackList.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_LABELTIP);
	m_CallStackList.SetImageList(images, LVSIL_SMALL);
	//m_CallStackList.SetFont(m_MonoFont);
	m_StackSplitter.SetSplitterPanes(m_ByStackList, m_CallStackList);
	m_Tabs.AddPage(m_StackSplitter, L"Call Stacks", 0);
	m_StackSplitter.SetSplitterPosPct(20);

	m_Tabs.SetActivePage(0);

	auto cm = GetColumnManager(m_AllList);
	if (cm) {
		cm->AddColumn(L"dummy", 0, 0, 0);
		cm->AddColumn(L"#", LVCFMT_RIGHT, 70, ColumnType::Index);
		cm->AddColumn(L"PID", LVCFMT_RIGHT, 60, ColumnType::PID);
		if (m_Session->IsRealTimeSession())
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
	}

	cm = GetColumnManager(m_SummaryList);
	cm->AddColumn(L"Name", 0, 200);
	cm->AddColumn(L"Value", 0, 500);
	m_SummaryList.SetItemCount(Event_Count + 5);

	cm = GetColumnManager(m_ByStackList);
	cm->AddColumn(L"dummy", 0, 0, 0);
	cm->AddColumn(L"Hash", LVCFMT_RIGHT, 100, ColumnType::Hash);
	cm->AddColumn(L"Count", LVCFMT_RIGHT, 100, ColumnType::Count);
	cm->AddColumn(L"Frames", LVCFMT_RIGHT, 60, ColumnType::Frames);
	cm->DeleteColumn(0);

	cm = GetColumnManager(m_CallStackList);
	cm->AddColumn(L"dummy", 0, 0, 0);
	cm->AddColumn(L"#", LVCFMT_RIGHT, 60, ColumnType::Index);
	cm->AddColumn(L"Address", LVCFMT_RIGHT, 120, ColumnType::Address);
	cm->AddColumn(L"Symbol", 0, 300, ColumnType::Symbol);
	cm->AddColumn(L"Source", 0, 350, ColumnType::Source);
	cm->DeleteColumn(0);

	return 0;
}

LRESULT CRomemView::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	std::unique_lock locker(m_EventsLock);
	if (m_TempEvents.empty()) {
		if (!m_Running)
			KillTimer(id);
	}
	else {
		for (auto& evt : m_TempEvents) {
			auto keyword = static_cast<Keyword>(evt->GetEventHeader().EventDescriptor.Keyword);
			if ((keyword & Keyword::Allocate) == Keyword::Allocate)
				m_Totals[Event_Op_Alloc]++;
			if ((keyword & Keyword::Free) == Keyword::Free)
				m_Totals[Event_Op_Free]++;
			if ((keyword & Keyword::Reallocate) == Keyword::Reallocate)
				m_Totals[Event_Op_Realloc]++;
			if ((keyword & Keyword::malloc) == Keyword::malloc)
				m_Totals[Event_malloc]++;
			if ((keyword & Keyword::free) == Keyword::free)
				m_Totals[Event_free]++;
			if ((keyword & Keyword::realloc) == Keyword::realloc)
				m_Totals[Event_realloc]++;
			if ((keyword & Keyword::RtlAllocateHeap) == Keyword::RtlAllocateHeap)
				m_Totals[Event_RtlAllocateHeap]++;
			if ((keyword & Keyword::RtlFreeHeap) == Keyword::RtlFreeHeap)
				m_Totals[Event_RtlFreeHeap]++;
			if ((keyword & Keyword::RtlReAllocateHeap) == Keyword::RtlReAllocateHeap)
				m_Totals[Event_RtlReAllocateHeap]++;
			if ((keyword & Keyword::aligned_malloc) == Keyword::aligned_malloc)
				m_Totals[Event_aligned_malloc]++;
			if ((keyword & Keyword::aligned_free) == Keyword::aligned_free)
				m_Totals[Event_aligned_free]++;
			if ((keyword & Keyword::aligned_realloc) == Keyword::aligned_realloc)
				m_Totals[Event_aligned_realloc]++;
			auto prop = evt->GetProperty(L"CallStackHash");
			if (prop) {
				auto hash = prop->GetValue<DWORD>();
				if (auto it = m_CallsByStack.find(hash); it != m_CallsByStack.end())
					it->second->Count++;
				else {
					auto cs = std::make_unique<CallStack>();
					cs->Count = 1;
					cs->Hash = hash;
					prop = evt->GetProperty(L"CallStack");
					auto len = prop->GetLength();
					auto count = len / sizeof(PVOID);
					cs->Frames.resize(count);
					auto data = (PVOID**)prop->GetData();
					for (int i = 0; i < count; i++) {
						cs->Frames[i].Index = i + 1;
						cs->Frames[i].Address = *(data + i);
					}
					m_CallsByStack.insert({ hash, cs.get() });
					m_CallStacks.push_back(std::move(cs));
				}
			}
		}
		m_Events.insert(m_Events.end(), m_TempEvents.begin(), m_TempEvents.end());
		m_TempEvents.clear();
		locker.unlock();

		if (m_FirstEvent == nullptr && (m_Events[0]->GetEventDescriptor().Keyword & (ULONGLONG)Keyword::ProcessInfo)) {
			m_FirstEvent = m_Events[0];
		}
		if (m_hProcess == nullptr) {
			if (m_Session->IsRealTimeSession()) {
				m_hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_Events[0]->GetProcessId());
			}
			else {
				m_hProcess = (HANDLE)1;
			}
			ATLVERIFY(::SymInitializeW(m_hProcess, nullptr, m_Session->IsRealTimeSession()));
		}

		if (m_AllList)
			m_AllList.SetItemCountEx((int)m_Events.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
		m_SummaryList.SetItemCountEx((int)m_SummaryList.GetItemCount(), LVSICF_NOSCROLL);
		Sort(m_ByStackList);
		m_ByStackList.SetItemCountEx((int)m_CallsByStack.size(), LVSICF_NOSCROLL);

		if (m_Active && m_AllList) {
			if (m_AutoScroll)
				m_AllList.EnsureVisible(m_AllList.GetItemCount() - 1, FALSE);
		}
		Frame()->SetStatusText(2, std::format(L"{} Events", m_Events.size()).c_str());
	}
	return 0;
}

LRESULT CRomemView::OnActivate(UINT, WPARAM active, LPARAM, BOOL&) {
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

LRESULT CRomemView::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
	m_Session->Stop();
	return 0;
}

LRESULT CRomemView::OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
	m_Tabs.SetFocus();
	return 0;
}

LRESULT CRomemView::OnQuickFind(WORD, WORD, HWND, BOOL&) {
	return LRESULT();
}

LRESULT CRomemView::OnRun(WORD, WORD, HWND, BOOL&) {
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

LRESULT CRomemView::OnStop(WORD, WORD, HWND, BOOL&) {
	if (!m_Running)
		return 0;

	m_Session->Pause(true);
	Frame()->UI().UISetCheck(ID_SESSION_RUN, false);
	Frame()->UI().UISetCheck(ID_SESSION_STOP, true);
	Frame()->SetStatusIcon(1, AtlLoadIconImage(IDI_STOP, 0, 16, 16));
	m_Running = false;

	return 0;
}

LRESULT CRomemView::OnAutoScroll(WORD, WORD, HWND, BOOL&) {
	m_AutoScroll = !m_AutoScroll;
	Frame()->UI().UISetCheck(ID_VIEW_AUTOSCROLL, m_AutoScroll);

	return 0;
}

LRESULT CRomemView::OnEditFilter(WORD, WORD, HWND, BOOL&) {
	auto mgr = m_FilterMgr.Clone();
	CFilterDlg dlg(mgr);
	if (IDOK == dlg.DoModal()) {
		m_FilterMgr = std::move(mgr);
	}

	return 0;
}

LRESULT CRomemView::OnViewProperties(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_AllList.GetSelectedCount() == 1);
	ShowProperties(m_AllList.GetNextItem(-1, LVNI_SELECTED));
	return 0;
}

LRESULT CRomemView::OnClearAll(WORD, WORD, HWND, BOOL&) {
	m_Events.clear();
	m_TempEvents.clear();
	m_AllList.SetItemCount(0);

	return 0;
}

LRESULT CRomemView::OnFileSave(WORD, WORD, HWND, BOOL&) const {
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

LRESULT CRomemView::OnCopy(WORD, WORD, HWND, BOOL&) const {
	ClipboardHelper::CopyText(m_hWnd, ListViewHelper::GetSelectedRowsAsString(m_AllList));
	return 0;
}


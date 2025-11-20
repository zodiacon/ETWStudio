// View.h : interface of the CRomemView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Interfaces.h"
#include <unordered_map>
#include <FrameView.h>
#include <VirtualListView.h>
#include <QuickFindEdit.h>
#include <TraceSession.h>
#include <EventData.h>
#include <FilterManager.h>
#include "resource.h"
#include <CustomTabView.h>
#include <CustomSplitterWindow.h>

class CRomemView :
	public CFrameView<CRomemView, IMainFrame>,
	public CVirtualListView<CRomemView> {
public:
	CRomemView(IMainFrame* frame, std::unique_ptr<TraceSession> session);

	CString GetColumnText(HWND, int row, int col) const;
	void DoSort(const SortInfo* si);
	int GetRowImage(HWND, int row, int col) const;
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);
	BOOL OnDoubleClickList(HWND h, int row, int col, POINT const& pt);
	BOOL OnRightClickList(HWND h, int row, int col, POINT const& pt);
	int GetSaveColumnRange(HWND, int&) const {
		return 1;
	}
	bool IsSortable(HWND h, int col) const;
	void ShowProperties(int index) const;

	BEGIN_MSG_MAP(CRomemView)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		MESSAGE_HANDLER(WM_ACTIVATE, OnActivate)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		CHAIN_MSG_MAP(CVirtualListView<CRomemView>)
		CHAIN_MSG_MAP(BaseFrame)
		ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_VIEW_PROPERTIES, OnViewProperties)
		COMMAND_ID_HANDLER(ID_SESSION_RUN, OnRun)
		COMMAND_ID_HANDLER(ID_SESSION_STOP, OnStop)
		COMMAND_ID_HANDLER(ID_EDIT_CLEAR_ALL, OnClearAll)
		COMMAND_ID_HANDLER(ID_VIEW_AUTOSCROLL, OnAutoScroll)
		COMMAND_ID_HANDLER(ID_EDIT_FILTER, OnEditFilter)
		COMMAND_ID_HANDLER(ID_FILE_SAVE, OnFileSave)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnCopy)
	END_MSG_MAP()

public:
	enum class Keyword : uint64_t {
		Allocate = 1,
		Reallocate = 2,
		Free = 4,
		MemoryBlocks = 8,
		ProcessInfo = 0x10,
		RtlAllocateHeap = 0x40,
		RtlFreeHeap = 0x80,
		RtlReAllocateHeap = 0x100,
		malloc = 0x200,
		free = 0x400,
		realloc = 0x800,
		aligned_malloc = 0x1000,
		aligned_free = 0x2000,
		aligned_realloc = 0x4000,
	};

private:
	void UpdateUI();
	bool DoSave(PCWSTR path) const;
	static std::wstring GetProperties(EventData const& evt);

	enum class ColumnType {
		ProcessName, Guid, Index,
		PID, TID, Time, Attributes, CPU,
		Keyword, Task, OpCode, Level, Message, Id,
		Source, Channel, Version, EventName, Properties, 
		Count, Hash, Frames, Address, Symbol,
	};

	enum EventType {
		Event_Op_Alloc,
		Event_Op_Free,
		Event_Op_Realloc,
		Event_RtlAllocateHeap,
		Event_RtlFreeHeap,
		Event_RtlReAllocateHeap,
		Event_malloc,
		Event_free,
		Event_realloc,
		Event_aligned_malloc,
		Event_aligned_free,
		Event_aligned_realloc,

		Event_Count
	};

	struct StackFrame {
		PVOID Address;
		CString Symbol;
		CString Source;
		int Index;
	};

	struct CallStack {
		int64_t Count;
		DWORD Hash;
		std::vector<StackFrame> Frames;
	};

	CString GetSymbol(PVOID address) const;
	CString GetSymbolSource(PVOID address) const;

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnActivate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnQuickFind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRun(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnStop(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAutoScroll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditFilter(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnClearAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFileSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;

	CCustomTabView m_Tabs;
	CListViewCtrl m_AllList, m_ByStackList, m_SummaryList, m_CallStackList;
	CCustomSplitterWindow m_StackSplitter;
	CQuickFindEdit m_QuickFind;
	CFont m_MonoFont;
	std::unique_ptr<TraceSession> m_Session;
	std::vector<std::shared_ptr<EventData>> m_Events;
	std::vector<std::shared_ptr<EventData>> m_TempEvents;
	std::shared_ptr<EventData> m_FirstEvent;
	int64_t m_Totals[Event_Count]{};
	std::unordered_map<DWORD, CallStack*> m_CallsByStack;
	std::vector<std::unique_ptr<CallStack>> m_CallStacks;
	std::mutex m_EventsLock;
	FilterManager m_FilterMgr;
	HANDLE m_hProcess{ nullptr };
	bool m_AutoScroll{ false };
	bool m_Running{ false };
	bool m_Active{ false };
};
DEFINE_ENUM_FLAG_OPERATORS(CRomemView::Keyword);

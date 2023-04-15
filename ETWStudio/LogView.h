// View.h : interface of the CLogView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Interfaces.h"
#include <FrameView.h>
#include <VirtualListView.h>
#include "EtwProvider.h"
#include <QuickFindEdit.h>
#include <TraceSession.h>
#include <SortedFilteredVector.h>
#include <EventData.h>
#include <FilterManager.h>

class CLogView :
	public CFrameView<CLogView, IMainFrame>,
	public CVirtualListView<CLogView> {
public:
	CLogView(IMainFrame* frame, std::unique_ptr<TraceSession> session);

	CString GetColumnText(HWND, int row, int col) const;
	void DoSort(const SortInfo* si);
	int GetRowImage(HWND, int row, int col) const;
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);
	BOOL OnDoubleClickList(HWND h, int row, int col, POINT const& pt);
	int GetSaveColumnRange(HWND, int&) const {
		return 1;
	}
	bool IsSortable(HWND h, int col) const;
	void ShowProperties(int index) const;

	BEGIN_MSG_MAP(CLogView)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		MESSAGE_HANDLER(WM_ACTIVATE, OnActivate)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		CHAIN_MSG_MAP(CVirtualListView<CLogView>)
		CHAIN_MSG_MAP(BaseFrame)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_VIEW_PROPERTIES, OnViewProperties)
		COMMAND_ID_HANDLER(ID_SESSION_RUN, OnRun)
		COMMAND_ID_HANDLER(ID_SESSION_STOP, OnStop)
		COMMAND_ID_HANDLER(ID_VIEW_AUTOSCROLL, OnAutoScroll)
		COMMAND_ID_HANDLER(ID_EDIT_FILTER, OnEditFilter)
	END_MSG_MAP()

private:
	void UpdateUI();

	enum class ColumnType {
		ProcessName, Guid, Index,
		PID, TID, Time, Attributes,
		Keyword, Task, OpCode, Level, Message, Id, 
		Source, Channel, Version, EventName,
	};

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

	CListViewCtrl m_List;
	CQuickFindEdit m_QuickFind;
	std::unique_ptr<TraceSession> m_Session;
	SortedFilteredVector<std::shared_ptr<EventData>> m_Events;
	std::vector<std::shared_ptr<EventData>> m_TempEvents;
	std::mutex m_EventsLock;
	FilterManager m_FilterMgr;
	bool m_AutoScroll{ false };
	bool m_Running{ false };
};

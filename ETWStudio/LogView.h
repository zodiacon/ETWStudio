// View.h : interface of the CLogView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Interfaces.h"
#include "ProgressDlg.h"
#include <FrameView.h>
#include <VirtualListView.h>
#include <QuickFindEdit.h>
#include <TraceSession.h>
#include <SortedFilteredVector.h>
#include <EventData.h>
#include <FilterManager.h>

// the quick find box: Enter/F3 find next, Shift+Enter/Shift+F3 previous, Esc closes.
// Reported to the parent as WM_COMMAND with these notification codes (and EN_DELAYCHANGE as typed).
class CQuickFindBox : public CQuickFindEdit {
public:
	enum { FindNext = 0x4001, FindPrevious, Close };

	BEGIN_MSG_MAP(CQuickFindBox)
		MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
		MESSAGE_HANDLER(WM_CHAR, OnChar)
		CHAIN_MSG_MAP(CQuickFindEdit)
	END_MSG_MAP()

private:
	void Notify(WORD code) {
		GetParent().SendMessage(WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), code), (LPARAM)m_hWnd);
	}
	LRESULT OnKeyDown(UINT, WPARAM key, LPARAM, BOOL& handled) {
		bool shift = ::GetKeyState(VK_SHIFT) < 0;
		switch (key) {
			case VK_RETURN: case VK_F3: Notify(shift ? FindPrevious : FindNext); return 0;
			case VK_ESCAPE: Notify(Close); return 0;
		}
		handled = FALSE;
		return 0;
	}
	LRESULT OnChar(UINT, WPARAM ch, LPARAM, BOOL& handled) {
		handled = ch == 0x0d || ch == 0x1b;	// handled on key down (no beep, no clearing)
		return 0;
	}
};

class CLogView :
	public CFrameView<CLogView, IMainFrame>,
	public CVirtualListView<CLogView> {
public:
	CLogView(IMainFrame* frame, std::unique_ptr<TraceSession> session);
	bool OpenLogFile(PCWSTR path);

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

	BEGIN_MSG_MAP(CLogView)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		MESSAGE_HANDLER(WM_ACTIVATE, OnActivate)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_UPDATE_FONT, OnUpdateFont)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		COMMAND_ID_HANDLER(QuickFindId, OnQuickFindNotify)
		NOTIFY_CODE_HANDLER(NM_CUSTOMDRAW, OnListCustomDraw)
		CHAIN_MSG_MAP(CVirtualListView<CLogView>)
		CHAIN_MSG_MAP(BaseFrame)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_VIEW_PROPERTIES, OnViewProperties)
		COMMAND_ID_HANDLER(ID_SESSION_RUN, OnRun)
		COMMAND_ID_HANDLER(ID_SESSION_STOP, OnStop)
		COMMAND_ID_HANDLER(ID_EDIT_CLEAR_ALL, OnClearAll)
		COMMAND_ID_HANDLER(ID_VIEW_AUTOSCROLL, OnAutoScroll)
		COMMAND_ID_HANDLER(ID_EDIT_FILTER, OnEditFilter)
		COMMAND_ID_HANDLER(ID_EDIT_HIGHLIGHT, OnEditHighlight)
		COMMAND_ID_HANDLER(ID_VIEW_QUICKFIND, OnQuickFind)
		COMMAND_ID_HANDLER(ID_VIEW_FIND, OnQuickFind)
		COMMAND_ID_HANDLER(ID_EDIT_FIND, OnQuickFind)
		COMMAND_ID_HANDLER(ID_FILE_SAVE, OnFileSave)
		COMMAND_ID_HANDLER(ID_FILE_SAVESESSION, OnSaveSession)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnCopy)
		COMMAND_ID_HANDLER(ID_SESSION_PROPERTIES, OnEditSession)
	END_MSG_MAP()

private:
	void UpdateUI();
	void CloseProperties();
	// re-evaluates the current filters against all events stored so far
	void ApplyFilterToExisting();
	// moves events not yet picked up by the timer into the list
	void AddPendingEvents();
	// searches the displayed text of all columns, wrapping around
	void QuickFind(bool forward, bool includeCurrent);
	void PositionQuickFind();
	// the color of the first enabled highlight rule matching the event, or CLR_INVALID
	COLORREF GetHighlightColor(EventData* evt) const;
	static std::wstring GetProperties(EventData const& evt);
	static std::wstring GetDetails(EventData const& evt);

	enum class ColumnType {
		ProcessName, Guid, Index,
		PID, TID, Time, Attributes, CPU,
		Keyword, Task, OpCode, Level, Message, Id, 
		Source, Channel, Version, EventName, Properties, Details,
	};
	CString GetEventText(EventData const& evt, ColumnType type) const;

	enum class SaveFormat { Csv, Json };
	// exports on a worker thread, with a progress dialog; a failed or canceled export leaves no file
	CProgressDlg::Result DoSave(PCWSTR path, SaveFormat format) const;
	bool SaveCsv(class Utf8Writer& writer, std::span<std::pair<std::wstring, ColumnType> const> columns,
		std::function<bool(size_t)> const& progress) const;

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnActivate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnUpdateFont(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnQuickFind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnQuickFindNotify(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditHighlight(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnListCustomDraw(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnRun(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnStop(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAutoScroll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditFilter(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnClearAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFileSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSaveSession(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnEditSession(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	enum { QuickFindId = 0x5001 };
	CQuickFindBox m_QuickFind;
	std::unique_ptr<TraceSession> m_Session;
	std::vector<EventData*> m_Events;
	std::vector<EventData*> m_TempEvents;
	std::mutex m_EventsLock;
	FilterManager m_FilterMgr;
	// PropertyFilter rules with a color; not applied to the session
	FilterManager m_Highlights;
	// properties dialogs hold borrowed event pointers; close them before events are freed
	mutable std::vector<HWND> m_PropertiesDialogs;
	bool m_AutoScroll{ false };
	bool m_ApplyFilterToExisting{ true };
	bool m_Running{ false };
	bool m_Active{ false };
	bool m_Done{ false };
};

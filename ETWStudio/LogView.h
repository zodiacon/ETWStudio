// View.h : interface of the CLogView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Interfaces.h"
#include <FrameView.h>
#include <VirtualListView.h>
#include "EtwProvider.h"
#include <QuickFindEdit.h>
#include "EtwSession.h"

class CLogView :
	public CFrameView<CLogView, IMainFrame>,
	public CVirtualListView<CLogView> {
public:
	CLogView(IMainFrame* frame, EtwSession session);

	CString GetColumnText(HWND, int row, int col) const;
	void DoSort(const SortInfo* si);
	int GetRowImage(HWND, int row, int col) const;
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);
	BOOL OnDoubleClickList(HWND h, int row, int col, POINT const& pt);

	BEGIN_MSG_MAP(CLogView)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CVirtualListView<CLogView>)
		CHAIN_MSG_MAP(BaseFrame)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_VIEW_QUICKFIND, OnQuickFind)
	ALT_MSG_MAP(2)
		MESSAGE_HANDLER(WM_CHAR, OnQuickEditChar)
	END_MSG_MAP()

private:
	enum class ColumnType {
		Name, Guid,	Type, Count,
		Keyword, Task, OpCode, Level, Message, Id, Source, ChannelName, Version,
	};

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnQuickFind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnQuickEditChar(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	CContainedWindowT<CQuickFindEdit> m_QuickFind;
	EtwSession m_Session;
};

// View.h : interface of the CProvidersView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Interfaces.h"
#include <FrameView.h>
#include <CustomSplitterWindow.h>
#include <TreeViewHelper.h>
#include <VirtualListView.h>
#include "EtwProvider.h"
#include <QuickFindEdit.h>

class CProvidersView :
	public CFrameView<CProvidersView, IMainFrame>,
	public CVirtualListView<CProvidersView> {
public:
	using CFrameView::CFrameView;

	BOOL PreTranslateMessage(MSG* pMsg);

	CString GetColumnText(HWND, int row, int col) const;
	CString GetPropertyText(int row, int col) const;
	void DoSort(const SortInfo* si);
	void DoSortProperties(const SortInfo* si);
	int GetRowImage(HWND, int row, int col) const;
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);
	//BOOL OnDoubleClickList(HWND h, int row, int col, POINT const& pt);

	BEGIN_MSG_MAP(CProvidersView)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CVirtualListView<CProvidersView>)
		CHAIN_MSG_MAP(BaseFrame)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_VIEW_QUICKFIND, OnQuickFind)
	ALT_MSG_MAP(2)
		MESSAGE_HANDLER(WM_CHAR, OnQuickEditChar)
	END_MSG_MAP()

private:
	enum class ColumnType {
		Name,
		Guid,
		Type, InType, OutType,
		Count,
		Keyword, Task, OpCode, Level, Message, Id, Source, ChannelName, Version,
	};
	
	void InitProviderList();

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnQuickFind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnQuickEditChar(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	CCustomSplitterWindow m_Splitter;
	CCustomHorSplitterWindow m_HSplitter;
	CListViewCtrl m_EventList;
	CListViewCtrl m_PropList;
	CListViewCtrl m_ProviderList;
	CContainedWindowT<CQuickFindEdit> m_QuickFind;
	std::vector<std::unique_ptr<EtwProvider>> m_Providers;
	std::vector<EVENT_DESCRIPTOR> m_Events;
	std::vector<EtwEventProperty> m_Properties;
	std::unordered_map<HTREEITEM, EtwProvider*> m_ProvidersMap;
};

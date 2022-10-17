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

class CProvidersView :
	public CFrameView<CProvidersView, IMainFrame>,
	public CVirtualListView<CProvidersView>,
	public CTreeViewHelper<CProvidersView> {
public:
	using CFrameView::CFrameView;

	BOOL PreTranslateMessage(MSG* pMsg);

	CString GetColumnText(HWND, int row, int col) const;
	CString GetPropertyText(int row, int col) const;
	void DoSort(const SortInfo* si);
	void DoSortProperties(const SortInfo* si);
	void OnTreeSelChanged(HWND tree, HTREEITEM hOld, HTREEITEM hNew);
	int GetRowImage(HWND, int row, int col) const;
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);

	BEGIN_MSG_MAP(CProvidersView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CVirtualListView<CProvidersView>)
		CHAIN_MSG_MAP(CTreeViewHelper<CProvidersView>)
		CHAIN_MSG_MAP(BaseFrame)
	END_MSG_MAP()

private:
	enum class ColumnType {
		Name,
		Guid,
		Type, InType, OutType,
		Count,
		Keyword, Task, OpCode, Level, Message, Id, Source, ChannelName,
	};

	enum class TreeIconType {
		Computer,
		Provider1,
		Provider2,
	};

	enum class TreeItemType {
		None,
		Root,
		Provider,
		_Count
	};
	void InitTree();
	void RefreshList(bool changeHeader);

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	CCustomSplitterWindow m_Splitter;
	CCustomHorSplitterWindow m_HSplitter;
	CListViewCtrl m_List;
	CListViewCtrl m_PropList;
	CTreeViewCtrl m_Tree;
	std::vector<std::unique_ptr<EtwProvider>> m_Providers;
	std::vector<EVENT_DESCRIPTOR> m_Events;
	std::vector<EtwEventProperty> m_Properties;
	std::unordered_map<HTREEITEM, EtwProvider*> m_ProvidersMap;
	TreeItemType m_CurrentNode{ TreeItemType::None };
	TreeItemType m_PreviousNode{ TreeItemType::None };
	ColumnsState m_ListViewState[(int)TreeItemType::_Count]{};
	ColumnManager m_ProviderCM, m_EventsCM;
	EtwProvider* m_CurrentProvider{ nullptr };
};

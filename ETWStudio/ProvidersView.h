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
	void DoSort(const SortInfo* si);
	void OnTreeSelChanged(HWND tree, HTREEITEM hOld, HTREEITEM hNew);

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
		Type,
		Count
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
		Events,
		Event,
		Keywords,
		_Count
	};
	void InitTree();
	void RefreshList();

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	CCustomSplitterWindow m_Splitter;
	CListViewCtrl m_List;
	CTreeViewCtrl m_Tree;
	std::vector<EtwProvider> m_Providers;
	TreeItemType m_CurrentNode{ TreeItemType::None };
	ColumnsState m_ListViewState[(int)TreeItemType::_Count]{};
};

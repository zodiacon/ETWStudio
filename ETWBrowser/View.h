// View.h : interface of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "VirtualListView.h"
#include "EtwProvider.h"

class CView : 
	public CFrameWindowImpl<CView, CWindow, CControlWinTraits>,
	public CCustomDraw<CView>,
	public CVirtualListView<CView> {
public:
	using BaseFrame = CFrameWindowImpl<CView, CWindow, CControlWinTraits>;

	DECLARE_WND_CLASS(nullptr)

	BOOL PreTranslateMessage(MSG* pMsg);

	CString GetColumnText(HWND, int row, int col) const;
	void DoSort(const SortInfo* si);

	BEGIN_MSG_MAP(CView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CVirtualListView<CView>)
		CHAIN_MSG_MAP(CCustomDraw<CView>)
		CHAIN_MSG_MAP(BaseFrame)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	void Refresh();

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	std::vector<EtwProvider> m_Providers;
};

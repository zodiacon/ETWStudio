// View.cpp : implementation of the CProvidersView class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "ProvidersView.h"

BOOL CProvidersView::PreTranslateMessage(MSG* pMsg) {
	pMsg;
	return FALSE;
}

void CProvidersView::OnFinalMessage(HWND /*hWnd*/) {
	delete this;
}

LRESULT CProvidersView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {

	return 0;
}

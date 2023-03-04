#include "pch.h"
#include "resource.h"
#include "LogView.h"

CLogView::CLogView(IMainFrame* frame, EtwSession session) : CFrameView(frame), m_Session(std::move(session)) {
}

CString CLogView::GetColumnText(HWND, int row, int col) const {
	return CString();
}

void CLogView::DoSort(const SortInfo* si) {
}

int CLogView::GetRowImage(HWND, int row, int col) const {
	return 0;
}

void CLogView::OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState) {
}

BOOL CLogView::OnDoubleClickList(HWND h, int row, int col, POINT const& pt) {
	return 0;
}

LRESULT CLogView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	return LRESULT();
}

LRESULT CLogView::OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
	m_List.SetFocus();
	return 0;
}

LRESULT CLogView::OnQuickFind(WORD, WORD, HWND, BOOL&) {
	return LRESULT();
}

LRESULT CLogView::OnQuickEditChar(UINT, WPARAM, LPARAM, BOOL&) {
	return LRESULT();
}

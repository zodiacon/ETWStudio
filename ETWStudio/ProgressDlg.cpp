#include "pch.h"
#include "ProgressDlg.h"

CProgressDlg::CProgressDlg(std::wstring title, std::wstring what, size_t total, Work work) :
	m_Title(std::move(title)), m_What(std::move(what)), m_Total(total), m_Work(std::move(work)) {
}

void CProgressDlg::SetDetail(std::function<std::wstring()> detail) {
	m_Detail = std::move(detail);
}

CProgressDlg* CProgressDlg::Running() {
	return s_Running;
}

CProgressDlg::Result CProgressDlg::Run(HWND owner) {
	auto outer = std::exchange(s_Running, this);
	DoModal(owner);
	s_Running = outer;
	// destroyed before the work was done (its owner went away): the worker still uses what it was given
	if (m_Thread.joinable()) {
		m_Cancel = true;
		m_Thread.join();
		m_Result = Result::Canceled;
	}
	// posted: handled once the caller has returned
	if (m_CloseWhenDone)
		::PostMessage(m_CloseWhenDone, WM_CLOSE, 0, 0);
	return m_Result;
}

void CProgressDlg::CancelAndClose(HWND window) {
	m_CloseWhenDone = window;
	Cancel();
}

LRESULT CProgressDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	SetWindowText(m_Title.c_str());
	CenterWindow(GetParent());
	m_Progress.Attach(GetDlgItem(IDC_PROGRESS));
	m_Progress.SetRange32(0, 1000);
	UpdateProgress();

	// short work finishes before the dialog would have been seen
	SetTimer(ShowTimer, 400);
	SetTimer(UpdateTimer, 100);
	m_Thread = std::thread([this, hDlg = m_hWnd] {
		bool ok = m_Work([this](size_t done) {
			m_Done = done;
			return !m_Cancel;
		});
		// not through m_hWnd: the dialog may be gone (see Run)
		::PostMessage(hDlg, WM_DONE, ok, 0);
	});
	return TRUE;
}

LRESULT CProgressDlg::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	if (id == ShowTimer) {
		KillTimer(ShowTimer);
		m_Show = true;
		ShowWindow(SW_SHOW);
	}
	else if (id == UpdateTimer) {
		UpdateProgress();
	}
	return 0;
}

// the dialog manager shows a modal dialog on its own once idle; until the timer, it stays hidden
LRESULT CProgressDlg::OnWindowPosChanging(UINT, WPARAM, LPARAM lParam, BOOL&) {
	if (!m_Show)
		((WINDOWPOS*)lParam)->flags &= ~SWP_SHOWWINDOW;
	return 0;
}

void CProgressDlg::UpdateProgress() {
	if (m_Cancel)
		return;
	size_t done = m_Done;
	m_Progress.SetPos(m_Total ? (int)(done * 1000 / m_Total) : 0);
	SetDlgItemText(IDC_PROGRESS_TEXT, std::format(L"{} {:L} of {:L}...", m_What, done, m_Total).c_str());
	if (m_Detail) {
		// only when it changes: the text would flicker otherwise
		if (auto detail = m_Detail(); detail != m_LastDetail) {
			SetDlgItemText(IDC_PROGRESS_DETAIL, detail.c_str());
			m_LastDetail = std::move(detail);
		}
	}
}

LRESULT CProgressDlg::OnDone(UINT, WPARAM ok, LPARAM, BOOL&) {
	m_Thread.join();
	m_Result = m_Cancel ? Result::Canceled : ok ? Result::Done : Result::Failed;
	EndDialog(IDOK);
	return 0;
}

LRESULT CProgressDlg::OnCancel(WORD, WORD, HWND, BOOL&) {
	Cancel();
	return 0;
}

void CProgressDlg::Cancel() {
	// the dialog closes when the worker has stopped (OnDone)
	m_Cancel = true;
	GetDlgItem(IDCANCEL).EnableWindow(FALSE);
	SetDlgItemText(IDC_PROGRESS_TEXT, L"Canceling...");
}

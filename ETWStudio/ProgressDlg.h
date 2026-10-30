#pragma once

#include "resource.h"
#include <DialogHelper.h>
#include <functional>
#include <thread>
#include <atomic>

// Runs work on a worker thread while the owner is disabled. The dialog only shows up when the work
// takes a while, with a progress bar and a Cancel button.
class CProgressDlg :
	public CDialogImpl<CProgressDlg>,
	public CDialogHelper<CProgressDlg> {
public:
	enum { IDD = IDD_PROGRESS };
	enum class Result { Done, Failed, Canceled };

	// runs on the worker thread: report(done) moves the bar and returns false once canceled;
	// returns false on failure (or when it stopped for a cancel)
	using Work = std::function<bool(std::function<bool(size_t)> const& report)>;

	CProgressDlg(std::wstring title, std::wstring what, size_t total, Work work);
	Result Run(HWND owner);
	// a line under the bar saying what the work is busy with (polled on the UI thread; empty: nothing to say)
	void SetDetail(std::function<std::wstring()> detail);

	// set when the work is to stop (Cancel, the app closing): for work that waits on something else meanwhile
	std::atomic<bool> const* CancelFlag() const noexcept {
		return &m_Cancel;
	}

	// the dialog running now (on the UI thread), if any
	static CProgressDlg* Running();
	// the app is closing: stops the work, and once it has (the dialog gone, so its caller has returned),
	// sends the window WM_CLOSE again. Closing the window meanwhile would destroy the dialog (and the view
	// the work belongs to) under the worker
	void CancelAndClose(HWND window);

	BEGIN_MSG_MAP(CProgressDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_WINDOWPOSCHANGING, OnWindowPosChanging)
		MESSAGE_HANDLER(WM_DONE, OnDone)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
	END_MSG_MAP()

private:
	enum { WM_DONE = WM_APP + 1, ShowTimer = 1, UpdateTimer = 2 };

	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnWindowPosChanging(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDone(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnCancel(WORD, WORD, HWND, BOOL&);
	void UpdateProgress();
	void Cancel();

	std::wstring m_Title, m_What;
	size_t m_Total;
	Work m_Work;
	std::function<std::wstring()> m_Detail;
	std::wstring m_LastDetail;
	std::thread m_Thread;
	std::atomic<size_t> m_Done{ 0 };
	std::atomic<bool> m_Cancel{ false };
	Result m_Result{ Result::Failed };
	bool m_Show{ false };
	HWND m_CloseWhenDone{ nullptr };
	static inline CProgressDlg* s_Running{ nullptr };
	CProgressBarCtrl m_Progress;
};

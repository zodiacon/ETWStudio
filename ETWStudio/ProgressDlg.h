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

	std::wstring m_Title, m_What;
	size_t m_Total;
	Work m_Work;
	std::thread m_Thread;
	std::atomic<size_t> m_Done{ 0 };
	std::atomic<bool> m_Cancel{ false };
	Result m_Result{ Result::Failed };
	bool m_Show{ false };
	CProgressBarCtrl m_Progress;
};

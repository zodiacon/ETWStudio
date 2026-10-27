#pragma once

#include "resource.h"
#include <DialogHelper.h>
#include <functional>

// Modeless Find dialog of a log view. It keeps its options while hidden; the view does the searching.
class CFindDlg :
	public CDialogImpl<CFindDlg>,
	public CDialogHelper<CFindDlg>,
	public CMessageFilter {
public:
	enum { IDD = IDD_FIND };

	struct Options {
		std::wstring Text;
		bool MatchCase{ false };
		bool WholeWord{ false };
		bool Properties{ false };	// also property names and values, not only the displayed columns
		bool Forward{ true };
	};
	enum class Action { FindNext, SelectAll };
	// does the search; returns the result to show in the dialog
	using Handler = std::function<std::wstring(Action, Options const&)>;

	explicit CFindDlg(Handler handler);
	// creates the dialog the first time, then shows and activates it
	void Show(HWND owner);
	// the options as last used (or set in the dialog)
	Options const& GetOptions() const;

	BOOL PreTranslateMessage(MSG* pMsg) override;

	BEGIN_MSG_MAP(CFindDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		COMMAND_ID_HANDLER(IDOK, OnFindNext)
		COMMAND_ID_HANDLER(IDC_FIND_SELECTALL, OnSelectAll)
		COMMAND_ID_HANDLER(IDCANCEL, OnClose)
		COMMAND_HANDLER(IDC_FIND_TEXT, EN_CHANGE, OnTextChanged)
	END_MSG_MAP()

private:
	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnFindNext(WORD, WORD, HWND, BOOL&);
	LRESULT OnSelectAll(WORD, WORD, HWND, BOOL&);
	LRESULT OnClose(WORD, WORD, HWND, BOOL&);
	LRESULT OnTextChanged(WORD, WORD, HWND, BOOL&);
	void ReadOptions();
	void Run(Action action);

	Handler m_Handler;
	Options m_Options;
};

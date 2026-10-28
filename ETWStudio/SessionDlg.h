#pragma once

#include "resource.h"
#include <DialogHelper.h>
#include <VirtualListView.h>
#include <TraceSession.h>
#include <EtwProvider.h>
#include <optional>
#include "SessionPreset.h"

struct IMainFrame;

class CSessionDlg :
	public CDialogImpl<CSessionDlg>,
	public CVirtualListView<CSessionDlg>,
	public CDynamicDialogLayout<CSessionDlg>,
	public CDialogHelper<CSessionDlg> {
public:
	enum { IDD = IDD_SESSION };

	CSessionDlg(IMainFrame* frame, TraceSession& session, bool edit = false);
	// a new session dialog starts with this preset instead of the session's settings
	void SetPreset(SessionPreset const& preset);

	CString GetColumnText(HWND, int row, int col) const;
	ListViewRowCheck IsRowChecked(HWND, int row) const;
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);
	void OnListViewClick(HWND, int row, int col, POINT const& pt);

	struct ProviderInfo {
		GUID Guid;
		std::wstring Name;
		ProviderConfig Config;		// for kernel rows, only Stacks is used
		bool MatchAll{ false };		// Config.MatchAllKeyword follows the checked keywords
		std::wstring EventIds;		// as entered (empty: all events)
		bool Kernel{ false };
		KernelEventTypes KernelType{ KernelEventTypes::None };
	};
	std::vector<ProviderInfo> const& GetProviders() const;

	// editing a running session: what was changed, and what failed (with the error)
	struct ApplyReport {
		std::vector<std::wstring> Changes, Failures;
	};
	ApplyReport const& GetApplyReport() const;

	// "1, 5, 10-20" -> IDs; empty text -> empty vector; invalid -> nullopt
	static std::optional<std::vector<USHORT>> ParseEventIds(std::wstring const& text);
	static std::wstring FormatEventIds(std::vector<USHORT> const& ids);

	BEGIN_MSG_MAP(CSessionDlg)
		NOTIFY_HANDLER(IDC_ADD, BCN_DROPDOWN, OnProviderDropdown)
		NOTIFY_HANDLER(IDC_LIST, LVN_KEYDOWN, OnProvidersKeyDown)
		NOTIFY_HANDLER(IDC_KEYWORDS, LVN_KEYDOWN, OnKeywordsKeyDown)
		COMMAND_ID_HANDLER(IDC_ADD, OnRegisteredProvider)
		COMMAND_ID_HANDLER(ID_PROVIDER_REGISTERED, OnRegisteredProvider)
		COMMAND_ID_HANDLER(ID_PROVIDER_GUID, OnGuidProvider)
		COMMAND_ID_HANDLER(ID_PROVIDER_KERNEL, OnKernelProvider)
		COMMAND_ID_HANDLER(IDC_BROWSE, OnBrowseFile)
		COMMAND_ID_HANDLER(IDC_REMOVE, OnRemoveProvider)
		COMMAND_HANDLER(IDC_LEVEL, CBN_SELCHANGE, OnLevelChanged)
		COMMAND_HANDLER(IDC_STACKS, BN_CLICKED, OnStacksChanged)
		COMMAND_HANDLER(IDC_MATCHALL, BN_CLICKED, OnMatchAllChanged)
		COMMAND_HANDLER(IDC_EVENTIDS, EN_CHANGE, OnEventIdsChanged)
		COMMAND_HANDLER(IDC_FILELOG, BN_CLICKED, OnFileLogChanged)
		COMMAND_ID_HANDLER(IDC_LOADSESSION, OnLoadPreset)
		COMMAND_ID_HANDLER(IDC_SAVESESSION, OnSavePreset)
		COMMAND_ID_HANDLER(IDC_SUMMARY, OnSummary)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CVirtualListView<CSessionDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CSessionDlg>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBrowseFile(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnProviderDropdown(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnProvidersKeyDown(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnKeywordsKeyDown(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnRegisteredProvider(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnGuidProvider(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnKernelProvider(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRemoveProvider(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnLevelChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnStacksChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnMatchAllChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEventIdsChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFileLogChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnLoadPreset(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSavePreset(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSummary(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	bool AddProvider(GUID const& guid, std::wstring name);
	ProviderInfo* GetSelectedProvider();
	void SelectProvider(int index);
	void UpdateSettings();
	void UpdateFileLog();
	void ToggleKeyword(int index);
	// redraws the selected rows
	void RefreshSelectedRows();
	// the keyword mask of the selected providers, if they all have the same one
	void ShowKeywordMask();
	std::vector<EtwFieldInfo> const& GetKeywords(GUID const& guid, std::wstring const& name);
	bool ApplyToSession(std::wstring const& name);
	// a running session: applies what changed in the dialog, filling m_Report
	void ApplyEdits();
	// selects the first provider with invalid event IDs and tells the user
	bool ValidateEventIds();
	// sessionSettings: also name, log file and buffers (not for a running session)
	void LoadPreset(SessionPreset const& preset, bool sessionSettings);
	SessionPreset GetPreset();
	BufferSettings GetBuffers();
	void SetBuffers(BufferSettings const& buffers);
	std::wstring GetSummary();

	IMainFrame* m_pFrame;
	TraceSession& m_Session;
	std::vector<ProviderInfo> m_Providers;
	// the selected rows' indices (call stacks apply to all of them)
	std::vector<int> GetSelectedRows() const;
	// the selected rows that are providers (not kernel rows): the other settings apply to these
	std::vector<int> GetSelectedProviderRows() const;
	std::unordered_map<GUID, std::vector<EtwFieldInfo>> m_KeywordCache;
	std::vector<EtwFieldInfo> const* m_Keywords{ nullptr };	// of the selected provider(s)
	// with several providers selected: the keywords of all of them (by name and value), and for each the
	// providers (rows) that have it
	std::vector<EtwFieldInfo> m_SharedKeywords;
	std::vector<std::vector<int>> m_SharedKeywordRows;
	CListViewCtrl m_List, m_KeywordList;
	CComboBox m_LevelCB;
	std::optional<SessionPreset> m_Preset;
	bool m_Edit;
	bool m_Updating{ false };
	ApplyReport m_Report;
};

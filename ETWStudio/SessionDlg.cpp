#include "pch.h"
#include "SessionDlg.h"
#include <WTLHelper.h>
#include "ProvidersDlg.h"
#include "Interfaces.h"
#include "StringHelper.h"
#include "SimpleDlg.h"
#include "KernelProviderDlg.h"

namespace {
	struct LevelInfo {
		PCWSTR Name;
		int Level;
	};
	const LevelInfo Levels[] = {
		{ L"Critical", TRACE_LEVEL_CRITICAL },
		{ L"Error", TRACE_LEVEL_ERROR },
		{ L"Warning", TRACE_LEVEL_WARNING },
		{ L"Information", TRACE_LEVEL_INFORMATION },
		{ L"Verbose", TRACE_LEVEL_VERBOSE },
	};

	PCWSTR LevelName(int level) {
		for (auto& l : Levels)
			if (l.Level == level)
				return l.Name;
		return L"";
	}
}

CSessionDlg::CSessionDlg(IMainFrame* frame, TraceSession& session, bool edit) : m_pFrame(frame), m_Session(session), m_Edit(edit) {
}

static CSessionDlg::ProviderInfo MakeKernelProvider(KernelEventTypes type) {
	CSessionDlg::ProviderInfo pi;
	pi.Kernel = true;
	pi.KernelType = type;
	pi.Guid = GUID_NULL;
	bool first = true;
	for (auto& cat : KernelEventCategory::GetAllCategories()) {
		if (cat.EnableFlag != type)
			continue;
		if (!first)
			pi.Name += L", ";
		pi.Name += cat.Name;
		if (first && cat.Guid)
			pi.Guid = *cat.Guid;
		first = false;
	}
	return pi;
}

std::optional<std::vector<USHORT>> CSessionDlg::ParseEventIds(std::wstring const& text) {
	std::vector<USHORT> ids;
	std::wstring token;
	auto addToken = [&]() -> bool {
		// trim
		auto start = token.find_first_not_of(L" \t");
		if (start == std::wstring::npos)
			return true;	// empty token (e.g. trailing comma)
		auto t = token.substr(start, token.find_last_not_of(L" \t") - start + 1);
		auto parse = [](std::wstring const& s) -> std::optional<ULONG> {
			if (s.empty() || !std::ranges::all_of(s, ::iswdigit))
				return {};
			auto value = std::wcstoul(s.c_str(), nullptr, 10);
			return value <= 0xffff ? std::optional<ULONG>(value) : std::nullopt;
		};
		auto dash = t.find(L'-');
		if (dash == std::wstring::npos) {
			auto value = parse(t);
			if (!value)
				return false;
			ids.push_back((USHORT)*value);
			return true;
		}
		auto trim = [](std::wstring s) {
			s.erase(0, s.find_first_not_of(L" \t"));
			s.erase(s.find_last_not_of(L" \t") + 1);
			return s;
		};
		auto from = parse(trim(t.substr(0, dash))), to = parse(trim(t.substr(dash + 1)));
		if (!from || !to || *from > *to)
			return false;
		for (auto id = *from; id <= *to; id++)
			ids.push_back((USHORT)id);
		return true;
	};
	for (auto ch : text) {
		if (ch == L',' || ch == L';') {
			if (!addToken())
				return {};
			token.clear();
		}
		else {
			token += ch;
		}
	}
	if (!addToken())
		return {};

	std::ranges::sort(ids);
	ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
	return ids;
}

std::wstring CSessionDlg::FormatEventIds(std::vector<USHORT> const& ids) {
	std::wstring text;
	for (size_t i = 0; i < ids.size(); i++) {
		auto start = ids[i];
		while (i + 1 < ids.size() && ids[i + 1] == ids[i] + 1)
			i++;
		if (!text.empty())
			text += L", ";
		text += start == ids[i] ? std::to_wstring(start) : std::format(L"{}-{}", start, ids[i]);
	}
	return text;
}

CString CSessionDlg::GetColumnText(HWND h, int row, int col) const {
	if (h == m_KeywordList) {
		auto& kw = (*m_Keywords)[row];
		switch (col) {
			case 0: return kw.Name.c_str();
			case 1: return std::format(L"0x{:X}", kw.Value).c_str();
			case 2: return kw.Desc.c_str();
		}
		return L"";
	}

	auto& pi = m_Providers[row];
	switch (col) {
		case 0: return pi.Kernel ? (pi.Name + L" (Kernel)").c_str() : pi.Name.c_str();
		case 1: return StringHelper::GuidToString(pi.Guid).c_str();
		case 2: return pi.Kernel ? L"" : LevelName(pi.Config.Level);
		case 3:
			if (pi.Kernel)
				return L"";
			return pi.Config.MatchAnyKeyword ? std::format(L"0x{:X}", pi.Config.MatchAnyKeyword).c_str() : L"All";
		case 4: return pi.Config.Stacks ? L"Yes" : L"";
		case 5: return pi.Kernel ? L"" : (pi.EventIds.empty() ? L"All" : pi.EventIds.c_str());
	}
	return L"";
}

ListViewRowCheck CSessionDlg::IsRowChecked(HWND h, int row) const {
	if (h != m_KeywordList)
		return ListViewRowCheck::None;
	auto pi = const_cast<CSessionDlg*>(this)->GetSelectedProvider();
	return pi && (pi->Config.MatchAnyKeyword & (*m_Keywords)[row].Value) ? ListViewRowCheck::Checked : ListViewRowCheck::Unchecked;
}

void CSessionDlg::OnStateChanged(HWND h, int from, int to, UINT oldState, UINT newState) {
	if (h == m_List && ((oldState ^ newState) & LVIS_SELECTED)) {
		GetDlgItem(IDC_REMOVE).EnableWindow(m_List.GetSelectedCount() > 0);
		UpdateSettings();
	}
}

void CSessionDlg::OnListViewClick(HWND h, int row, int, POINT const& pt) {
	if (h != m_KeywordList || row < 0)
		return;
	LVHITTESTINFO info{};
	info.pt = pt;
	m_KeywordList.HitTest(&info);
	if (info.iItem == row && (info.flags & LVHT_ONITEMSTATEICON))
		ToggleKeyword(row);
}

std::vector<CSessionDlg::ProviderInfo> const& CSessionDlg::GetProviders() const {
	return m_Providers;
}

CSessionDlg::ProviderInfo* CSessionDlg::GetSelectedProvider() {
	if (m_List.GetSelectedCount() != 1)
		return nullptr;
	return &m_Providers[m_List.GetNextItem(-1, LVNI_SELECTED)];
}

void CSessionDlg::SelectProvider(int index) {
	m_List.SetItemState(-1, 0, LVIS_SELECTED);
	m_List.SelectItem(index);
	m_List.EnsureVisible(index, FALSE);
	UpdateSettings();
}

std::vector<EtwFieldInfo> const& CSessionDlg::GetKeywords(GUID const& guid, std::wstring const& name) {
	auto it = m_KeywordCache.find(guid);
	if (it == m_KeywordCache.end()) {
		EtwProvider provider(guid, name.c_str(), EtwSchemaSource::Xml);
		auto keywords = provider.FieldInfo(EtwFieldType::KeywordInformation);
		std::ranges::sort(keywords, [](auto& k1, auto& k2) { return k1.Value < k2.Value; });
		it = m_KeywordCache.insert({ guid, std::move(keywords) }).first;
	}
	return it->second;
}

void CSessionDlg::UpdateSettings() {
	m_Updating = true;
	auto pi = GetSelectedProvider();
	bool provider = pi && !pi->Kernel;

	CString caption = L"Provider Settings";
	if (pi)
		caption = (L"Settings: " + pi->Name + (pi->Kernel ? L" (Kernel)" : L"")).c_str();
	else if (m_List.GetSelectedCount() > 1)
		caption = L"Provider Settings (select a single provider)";
	SetDlgItemText(IDC_SETTINGS, caption);

	m_LevelCB.EnableWindow(provider);
	m_KeywordList.EnableWindow(provider);
	GetDlgItem(IDC_EVENTIDS).EnableWindow(provider);
	GetDlgItem(IDC_STACKS).EnableWindow(pi != nullptr);

	m_LevelCB.SetCurSel(-1);
	if (provider) {
		for (int i = 0; i < m_LevelCB.GetCount(); i++)
			if ((int)m_LevelCB.GetItemData(i) == pi->Config.Level)
				m_LevelCB.SetCurSel(i);
	}
	CheckDlgButton(IDC_STACKS, pi && pi->Config.Stacks ? BST_CHECKED : BST_UNCHECKED);
	SetDlgItemText(IDC_EVENTIDS, provider ? pi->EventIds.c_str() : L"");

	static const std::vector<EtwFieldInfo> empty;
	m_Keywords = provider ? &GetKeywords(pi->Guid, pi->Name) : &empty;
	m_KeywordList.SetItemCount((int)m_Keywords->size());
	m_KeywordList.RedrawItems(0, m_KeywordList.GetItemCount());
	SetDlgItemText(IDC_KEYWORDMASK, provider ? std::format(L"0x{:016X}", pi->Config.MatchAnyKeyword).c_str() : L"");
	m_Updating = false;
}

void CSessionDlg::RefreshSelectedRow() {
	int index = m_List.GetNextItem(-1, LVNI_SELECTED);
	if (index >= 0)
		m_List.RedrawItems(index, index);
}

void CSessionDlg::ToggleKeyword(int index) {
	auto pi = GetSelectedProvider();
	if (!pi || pi->Kernel)
		return;
	pi->Config.MatchAnyKeyword ^= (*m_Keywords)[index].Value;
	m_KeywordList.RedrawItems(0, m_KeywordList.GetItemCount());	// keywords may share bits
	SetDlgItemText(IDC_KEYWORDMASK, std::format(L"0x{:016X}", pi->Config.MatchAnyKeyword).c_str());
	RefreshSelectedRow();
}

void CSessionDlg::UpdateFileLog() {
	bool enable = !m_Edit && IsDlgButtonChecked(IDC_FILELOG);
	GetDlgItem(IDC_PATH).EnableWindow(enable);
	GetDlgItem(IDC_BROWSE).EnableWindow(enable);
}

LRESULT CSessionDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout();
	SetDialogIcon(IDI_SESSION);

	SetDlgItemText(IDC_NAME, m_Session.SessionName().c_str());

	m_List.Attach(GetDlgItem(IDC_LIST));
	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Name", 0, 200);
	cm->AddColumn(L"GUID", 0, 230);
	cm->AddColumn(L"Level", 0, 70);
	cm->AddColumn(L"Keywords", 0, 110);
	cm->AddColumn(L"Stacks", 0, 45);
	cm->AddColumn(L"Events", 0, 80);

	m_KeywordList.Attach(GetDlgItem(IDC_KEYWORDS));
	m_KeywordList.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_CHECKBOXES);
	cm = GetColumnManager(m_KeywordList);
	cm->AddColumn(L"Keyword", 0, 200);
	cm->AddColumn(L"Value", 0, 120);
	cm->AddColumn(L"Description", 0, 200);

	m_LevelCB.Attach(GetDlgItem(IDC_LEVEL));
	for (auto& level : Levels)
		m_LevelCB.SetItemData(m_LevelCB.AddString(level.Name), level.Level);

	if (m_Edit) {
		for (auto& [guid, config] : m_Session.GetProviders()) {
			ProviderInfo pi;
			pi.Guid = guid;
			pi.Config = config;
			pi.Name = StringHelper::ProviderGuidToName(guid);
			pi.EventIds = FormatEventIds(m_Session.GetEventsForProvider(guid));
			m_Providers.push_back(std::move(pi));
		}
		auto stacks = m_Session.GetKernelEventStacks();
		for (auto type : m_Session.GetKernelEventTypes()) {
			auto pi = MakeKernelProvider(type);
			for (auto& cat : KernelEventCategory::GetAllCategories())
				if (cat.EnableFlag == type && std::ranges::find(stacks, cat.Name) != stacks.end())
					pi.Config.Stacks = true;
			m_Providers.push_back(std::move(pi));
		}
		m_List.SetItemCount((int)m_Providers.size());
		SetWindowText((L"Edit Session: " + m_Session.SessionName()).c_str());
		((CEdit)GetDlgItem(IDC_NAME)).SetReadOnly(TRUE);
		if (!m_Session.OutputFile().empty()) {
			SetDlgItemText(IDC_PATH, m_Session.OutputFile().c_str());
			CheckDlgButton(IDC_FILELOG, BST_CHECKED);
		}
		GetDlgItem(IDC_FILELOG).EnableWindow(FALSE);
	}
	GetDlgItem(IDC_REMOVE).EnableWindow(FALSE);
	UpdateFileLog();
	UpdateSettings();
	return 0;
}

bool CSessionDlg::ApplyToSession(std::wstring const& name) {
	if (!m_Edit)
		m_Session.SetSessionName(name);

	if (m_Edit) {
		// providers removed in the dialog
		for (auto& [guid, config] : m_Session.GetProviders()) {
			if (std::ranges::none_of(m_Providers, [&](auto& p) { return !p.Kernel && p.Guid == guid; }))
				m_Session.RemoveProvider(guid);
		}
	}

	std::vector<KernelEventTypes> kernelTypes;
	std::vector<std::wstring> stackCategories;
	for (auto& p : m_Providers) {
		if (p.Kernel) {
			kernelTypes.push_back(p.KernelType);
			if (p.Config.Stacks) {
				for (auto& cat : KernelEventCategory::GetAllCategories())
					if (cat.EnableFlag == p.KernelType)
						stackCategories.push_back(cat.Name);
			}
			continue;
		}
		m_Session.AddProvider(p.Guid, p.Config);
		m_Session.AddEventsForProvider(p.Guid, *ParseEventIds(p.EventIds));
	}

	if (m_Edit && !m_Session.IsKernelSession()) {
		if (!kernelTypes.empty())
			AtlMessageBox(m_hWnd, L"Kernel events can only be changed for sessions that were started with kernel events.\n"
				L"Create a new session to trace kernel events.", IDS_TITLE, MB_ICONWARNING);
		return true;
	}
	m_Session.SetKernelEventTypes(kernelTypes);
	m_Session.SetKernelEventStacks(stackCategories);
	if (m_Edit)
		m_Session.UpdateEventConfig();

	if (!m_Edit && IsDlgButtonChecked(IDC_FILELOG)) {
		CString path;
		GetDlgItemText(IDC_PATH, path);
		m_Session.SetOutputFile((PCWSTR)path);
	}
	return true;
}

LRESULT CSessionDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	if (wID == IDOK) {
		CString name;
		GetDlgItemText(IDC_NAME, name);
		name.Trim();
		if (name.IsEmpty()) {
			AtlMessageBox(m_hWnd, L"Please enter a session name", IDS_TITLE, MB_ICONWARNING);
			GetDlgItem(IDC_NAME).SetFocus();
			return 0;
		}
		if (m_Providers.empty()) {
			AtlMessageBox(m_hWnd, L"Session must have at least one provider",
				IDS_TITLE, MB_ICONWARNING);
			return 0;
		}
		for (int i = 0; i < (int)m_Providers.size(); i++) {
			auto& p = m_Providers[i];
			if (!p.Kernel && !ParseEventIds(p.EventIds)) {
				SelectProvider(i);
				AtlMessageBox(m_hWnd, std::format(L"Invalid event IDs for {}.\nUse numbers and ranges, e.g. 1, 5, 10-20", p.Name).c_str(),
					IDS_TITLE, MB_ICONWARNING);
				GetDlgItem(IDC_EVENTIDS).SetFocus();
				return 0;
			}
		}
		if (!m_Edit && IsDlgButtonChecked(IDC_FILELOG) && GetDlgItem(IDC_PATH).GetWindowTextLength() == 0) {
			AtlMessageBox(m_hWnd, L"Please specify a file to log to", IDS_TITLE, MB_ICONWARNING);
			GetDlgItem(IDC_PATH).SetFocus();
			return 0;
		}
		if (!m_Edit && TraceSession::SessionExists(name)) {
			auto text = std::format(L"A trace session named \"{}\" is already running (it may belong to another tool).\n\n"
				L"Stop it and use this name?", (PCWSTR)name);
			if (AtlMessageBox(m_hWnd, text.c_str(), IDS_TITLE, MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES) {
				GetDlgItem(IDC_NAME).SetFocus();
				return 0;
			}
		}
		ApplyToSession((PCWSTR)name);
	}
	EndDialog(wID);
	return 0;
}

LRESULT CSessionDlg::OnBrowseFile(WORD, WORD wID, HWND, BOOL&) {
	CSimpleFileDialog dlg(FALSE, L"etl", L"log", OFN_ENABLESIZING | OFN_EXPLORER | OFN_OVERWRITEPROMPT,
		L"Event Tracing Log files (*.etl)\0*.etl*\0All Files\0*.*\0", m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = IDOK == dlg.DoModal();
	WTLHelper::ResumeHook();
	if (ok) {
		SetDlgItemText(IDC_PATH, dlg.m_szFileName);
	}
	return 0;
}

LRESULT CSessionDlg::OnProviderDropdown(int, LPNMHDR, BOOL&) {
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	CRect rc;
	GetDlgItem(IDC_ADD).GetWindowRect(&rc);
	auto cmd = m_pFrame->DisplayContextMenu(menu.GetSubMenu(0), rc.left, rc.bottom, TPM_RETURNCMD);
	if (cmd) {
		LRESULT result;
		ProcessWindowMessage(m_hWnd, WM_COMMAND, cmd, 0, result);
	}
	return 0;
}

LRESULT CSessionDlg::OnProvidersKeyDown(int, LPNMHDR hdr, BOOL&) {
	if (((NMLVKEYDOWN*)hdr)->wVKey == VK_DELETE && m_List.GetSelectedCount() > 0)
		SendMessage(WM_COMMAND, IDC_REMOVE);
	return 0;
}

LRESULT CSessionDlg::OnKeywordsKeyDown(int, LPNMHDR hdr, BOOL&) {
	int index = m_KeywordList.GetSelectedIndex();
	if (((NMLVKEYDOWN*)hdr)->wVKey == VK_SPACE && index >= 0)
		ToggleKeyword(index);
	return 0;
}

bool CSessionDlg::AddProvider(GUID const& guid, std::wstring name) {
	if (std::ranges::any_of(m_Providers, [&](auto& p) { return !p.Kernel && p.Guid == guid; }))
		return false;

	ProviderInfo pi;
	pi.Guid = guid;
	pi.Name = std::move(name);
	m_Providers.push_back(std::move(pi));
	m_List.SetItemCountEx((int)m_Providers.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	return true;
}

LRESULT CSessionDlg::OnRegisteredProvider(WORD, WORD wID, HWND, BOOL&) {
	CProvidersDlg dlg;
	if (IDOK == dlg.DoModal()) {
		int first = (int)m_Providers.size();
		for (auto p : dlg.GetSelectedProviders())
			AddProvider(p->Guid(), p->Name());
		if (first < (int)m_Providers.size())
			SelectProvider(first);
	}
	return 0;
}

LRESULT CSessionDlg::OnGuidProvider(WORD, WORD wID, HWND, BOOL&) {
	CSimpleDlg dlg;
	if (IDOK == dlg.DoModal()) {
		CString text(dlg.GetText());
		text.Trim();
		if (!text.IsEmpty() && text[0] != L'{')
			text = L"{" + text + L"}";
		GUID guid;
		if (S_OK != ::CLSIDFromString(text, &guid)) {
			AtlMessageBox(m_hWnd, L"Invalid GUID", IDS_TITLE, MB_ICONWARNING);
			return 0;
		}
		// registered providers get their name; others show the GUID
		if (AddProvider(guid, StringHelper::ProviderGuidToName(guid)))
			SelectProvider((int)m_Providers.size() - 1);
		else
			AtlMessageBox(m_hWnd, L"This provider is already in the session", IDS_TITLE, MB_ICONINFORMATION);
	}
	return 0;
}

LRESULT CSessionDlg::OnKernelProvider(WORD, WORD wID, HWND, BOOL&) {
	std::vector<KernelEventTypes> current;
	std::unordered_set<KernelEventTypes> stacks;
	for (auto& p : m_Providers) {
		if (p.Kernel) {
			current.push_back(p.KernelType);
			if (p.Config.Stacks)
				stacks.insert(p.KernelType);
		}
	}

	CKernelProviderDlg dlg(current);
	if (IDOK == dlg.DoModal()) {
		std::erase_if(m_Providers, [](auto& p) { return p.Kernel; });
		for (auto type : dlg.GetSelectedTypes()) {
			auto pi = MakeKernelProvider(type);
			pi.Config.Stacks = stacks.contains(type);
			m_Providers.push_back(std::move(pi));
		}
		m_List.SetItemState(-1, 0, LVIS_SELECTED);
		m_List.SetItemCountEx((int)m_Providers.size(), LVSICF_NOSCROLL);
		m_List.RedrawItems(0, m_List.GetItemCount());
		UpdateSettings();
	}
	return 0;
}

LRESULT CSessionDlg::OnRemoveProvider(WORD, WORD wID, HWND, BOOL&) {
	std::vector<int> selected;
	for (int index = m_List.GetNextItem(-1, LVNI_SELECTED); index >= 0; index = m_List.GetNextItem(index, LVNI_SELECTED))
		selected.push_back(index);
	if (selected.empty())
		return 0;

	for (auto it = selected.rbegin(); it != selected.rend(); ++it)
		m_Providers.erase(m_Providers.begin() + *it);
	m_List.SetItemState(-1, 0, LVIS_SELECTED);
	m_List.SetItemCountEx((int)m_Providers.size(), LVSICF_NOSCROLL);
	m_List.RedrawItems(0, m_List.GetItemCount());
	if (!m_Providers.empty())
		SelectProvider((std::min)(selected[0], (int)m_Providers.size() - 1));
	else
		UpdateSettings();
	GetDlgItem(IDC_REMOVE).EnableWindow(m_List.GetSelectedCount() > 0);
	return 0;
}

LRESULT CSessionDlg::OnLevelChanged(WORD, WORD, HWND, BOOL&) {
	auto pi = GetSelectedProvider();
	if (m_Updating || !pi || pi->Kernel || m_LevelCB.GetCurSel() < 0)
		return 0;
	pi->Config.Level = (int)m_LevelCB.GetItemData(m_LevelCB.GetCurSel());
	RefreshSelectedRow();
	return 0;
}

LRESULT CSessionDlg::OnStacksChanged(WORD, WORD, HWND, BOOL&) {
	auto pi = GetSelectedProvider();
	if (m_Updating || !pi)
		return 0;
	pi->Config.Stacks = IsDlgButtonChecked(IDC_STACKS) == BST_CHECKED;
	RefreshSelectedRow();
	return 0;
}

LRESULT CSessionDlg::OnEventIdsChanged(WORD, WORD, HWND, BOOL&) {
	auto pi = GetSelectedProvider();
	if (m_Updating || !pi || pi->Kernel)
		return 0;
	CString text;
	GetDlgItemText(IDC_EVENTIDS, text);
	pi->EventIds = (PCWSTR)text.Trim();
	RefreshSelectedRow();
	return 0;
}

LRESULT CSessionDlg::OnFileLogChanged(WORD, WORD, HWND, BOOL&) {
	UpdateFileLog();
	return 0;
}

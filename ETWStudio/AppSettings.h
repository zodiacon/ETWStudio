#pragma once

#include <Settings.h>

class AppSettings : public Settings {
public:
	BEGIN_SETTINGS(AppSettings)
		SETTING(MainWindowPlacement, WINDOWPLACEMENT{}, SettingType::Binary);
		SETTING(Font, LOGFONT{}, SettingType::Binary);
		SETTING(AlwaysOnTop, 0, SettingType::Bool);
		SETTING(ViewStatusBar, 1, SettingType::Bool);
		SETTING(DarkMode, 0, SettingType::Bool);
		SETTING(SymbolPath, L"", SettingType::String);		// before the symbol settings below; moved to SymbolExtraPaths
		SETTING(SymbolServerEnabled, 0, SettingType::Bool);
		// SETTING_STRING copies the text; SETTING with a PCWSTR would store the pointer's bytes
		SETTING_STRING(SymbolServerUrl, DefaultSymbolServerUrl);
		SETTING_STRING(SymbolCache, DefaultSymbolCache);
		SETTING_STRING(SymbolExtraPaths, L"");
		SETTING(SymbolUseEnvPath, 1, SettingType::Bool);
		SETTING_STRING(RecentSessions, L"");		// session file paths, most recent first, one per line
		SETTING_STRING(RecentFiles, L"");			// opened data (.etl) files, the same way
		SETTING(ExportCsvStacks, 0, SettingType::Bool);		// CSV exports get a Stack column
		SETTING(ExportStackSymbols, 1, SettingType::Bool);	// exported stacks get their symbols (may download them)
		END_SETTINGS

	DEF_SETTING(DarkMode, bool)
	DEF_SETTING(Font, LOGFONT)
	DEF_SETTING(AlwaysOnTop, bool)
	DEF_SETTING(ViewStatusBar, bool)
	DEF_SETTING(MainWindowPlacement, WINDOWPLACEMENT)
	DEF_SETTING_STRING(SymbolPath)
	DEF_SETTING(SymbolServerEnabled, bool)
	DEF_SETTING_STRING(SymbolServerUrl)
	DEF_SETTING_STRING(SymbolCache)
	DEF_SETTING_STRING(SymbolExtraPaths)
	DEF_SETTING(SymbolUseEnvPath, bool)
	DEF_SETTING_STRING(RecentSessions)
	DEF_SETTING_STRING(RecentFiles)
	DEF_SETTING(ExportCsvStacks, bool)
	DEF_SETTING(ExportStackSymbols, bool)

	// the recently used files of each kind (File > Recent Files, Recent Sessions), most recent first
	enum class Recent { Files, Sessions };
	static constexpr int MaxRecent = 8;
	std::vector<std::wstring> RecentList(Recent kind) const;
	// moves the path to the top (adding it if new); remove: takes it out
	void UpdateRecent(Recent kind, std::wstring const& path, bool remove = false);
	void ClearRecent(Recent kind);

	static constexpr PCWSTR DefaultSymbolServerUrl = L"https://msdl.microsoft.com/download/symbols";
	static constexpr PCWSTR DefaultSymbolCache = L"%LOCALAPPDATA%\\ETWStudio\\Symbols";

	// after loading: a symbol path from an older version becomes the additional paths
	void Upgrade();

	// the search path handed to DbgHelp ("extra paths;_NT_SYMBOL_PATH;srv*cache*url"); empty: none
	std::wstring BuildSymbolSearchPath() const;
	static std::wstring BuildSymbolSearchPath(bool useServer, std::wstring const& url, std::wstring const& cache,
		std::wstring const& extra, bool useEnv);
};


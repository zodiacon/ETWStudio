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
		SETTING(SymbolPath, L"", SettingType::String);
		END_SETTINGS

	DEF_SETTING(DarkMode, bool)
	DEF_SETTING(AlwaysOnTop, bool)
	DEF_SETTING(ViewStatusBar, bool)
	DEF_SETTING(MainWindowPlacement, WINDOWPLACEMENT)
	DEF_SETTING(SymbolPath, CString)
};


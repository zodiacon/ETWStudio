#pragma once

// sent by the frame to every page (HFONT in wParam) when the user changes the font
constexpr UINT WM_UPDATE_FONT = WM_APP + 200;

struct IMainFrame abstract {
	virtual UINT DisplayContextMenu(HMENU hMenu, int x, int y, DWORD flags = 0) = 0;
	virtual CUpdateUIBase& UI() = 0;
	virtual void SetStatusText(int pane, PCWSTR text) = 0;
	virtual void SetStatusIcon(int pane, HICON hIcon) = 0;
	virtual HFONT GetFont() const = 0;
};

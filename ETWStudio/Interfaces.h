#pragma once

struct IMainFrame abstract {
	virtual UINT DisplayContextMenu(HMENU hMenu, int x, int y, DWORD flags = 0) = 0;
	virtual CUpdateUIBase& UI() = 0;
};

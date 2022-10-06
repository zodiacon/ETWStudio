#pragma once

struct IMainFrame abstract {
	virtual bool DisplayContextMenu(HMENU hMenu, int x, int y, DWORD flags = 0) = 0;
};

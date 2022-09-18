#pragma once

struct IListViewProvider abstract {
	virtual int GetColumnCount() = 0;
	virtual CString GetColumnTitle(int column) = 0;
	virtual CString GetColumnText(int row, int column) = 0;
	virtual HICON GetRowIcon(int row) = 0;

	virtual void OnRightClick(int row, int column) {}
	virtual void OnDoubleClick(int row, int column) {}
};

struct TreeNodeBase abstract {
};


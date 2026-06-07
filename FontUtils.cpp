#include "NativeSurface.h"
#include "FontUtils.h"

namespace
{
	void applyFontToControl(HWND ctrl, LPARAM fontHandle)
	{
		SendMessage(ctrl, WM_SETFONT, static_cast<WPARAM>(fontHandle), TRUE);
	}

	BOOL CALLBACK propagateFont(HWND child, LPARAM fontHandle)
	{
		applyFontToControl(child, fontHandle);
		return TRUE;
	}
}

void setFontHeader(const HWND hControl)
{
	const HFONT hFont = CreateFont(
		19, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
	);

	applyFontToControl(hControl, reinterpret_cast<LPARAM>(hFont));
}

void setFontChildren(const HWND hWnd)
{
	const HFONT hFont = CreateFont(
		16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
	);

	EnumChildWindows(hWnd, propagateFont, reinterpret_cast<LPARAM>(hFont));
}

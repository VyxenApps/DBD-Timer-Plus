#include "PrefsPanel.h"
#include "BrushFactory.h"
#include "FontUtils.h"
#include "JsonPersistence.h"
#include "LoadoutEditor.h"
#include "SpinnerWheel.h"
#include "NotifyBubble.h"
#include "NativeSurface.h"
#include "EntryPoint.h"
#include "OverlayWindow.h"
#include "GamepadPoller.h"
#include "AppResources.h"
#include <gdiplus.h>
#include <algorithm>
#include <climits>
#include <CommCtrl.h>
#include <commdlg.h>
#include <cwctype>
#include <sstream>
#include <vector>
#include <windowsx.h>
#include <exception>

#ifdef max
#undef max
#endif

#pragma comment(lib, "gdiplus.lib")

namespace
{
	constexpr int kWheelPresetCount = 5;
	constexpr int kStreakPresetCount = 2;
	constexpr COLORREF kPresetRenameFill = RGB(239, 248, 255);
	constexpr COLORREF kPresetRenameText = RGB(5, 14, 22);
	constexpr COLORREF kBgTop = RGB(7, 12, 19);
	constexpr COLORREF kBgBottom = RGB(13, 18, 28);
	constexpr COLORREF kCardFill = RGB(19, 26, 38);
	constexpr COLORREF kCardBorder = RGB(48, 67, 92);
	constexpr COLORREF kAccent = RGB(33, 159, 240);
	constexpr COLORREF kAccentSoft = RGB(136, 219, 255);
	constexpr COLORREF kTextPrimary = RGB(232, 239, 247);
	constexpr COLORREF kTextMuted = RGB(145, 166, 190);
	constexpr COLORREF kInputFill = RGB(10, 16, 24);
	constexpr COLORREF kButtonFill = RGB(28, 37, 52);
	constexpr COLORREF kButtonFillPressed = RGB(36, 51, 72);
	constexpr COLORREF kScrollTrackFill = RGB(14, 20, 30);
	constexpr COLORREF kScrollThumbFill = RGB(42, 174, 247);
	constexpr COLORREF kScrollThumbBorder = RGB(168, 229, 255);
	constexpr COLORREF kScrollThumbDormant = RGB(52, 63, 80);
	constexpr COLORREF kCancelFill = RGB(23, 29, 41);
	constexpr COLORREF kCheckBorder = RGB(88, 110, 135);
	constexpr COLORREF kOkFill = RGB(38, 170, 245);
	constexpr COLORREF kTabDormantFill = RGB(16, 22, 33);
	constexpr COLORREF kWheelCardFill = RGB(18, 25, 37);

	bool textEquals(const wchar_t* left, const wchar_t* right)
	{
		return left != nullptr && right != nullptr && wcscmp(left, right) == 0;
	}

	bool textStartsWith(const wchar_t* text, const wchar_t* prefix)
	{
		return text != nullptr && prefix != nullptr && wcsncmp(text, prefix, wcslen(prefix)) == 0;
	}

	std::wstring stripWs(const std::wstring& text)
	{
		const size_t first = text.find_first_not_of(L" \t\r\n");
		if (first == std::wstring::npos) {
			return L"";
		}

		const size_t last = text.find_last_not_of(L" \t\r\n");
		return text.substr(first, last - first + 1);
	}

	std::wstring getLineOrZero(const std::wstring& text, const int lineIndex)
	{
		std::wstringstream stream(text);
		std::wstring line;
		for (int index = 0; index <= lineIndex; ++index)
		{
			if (!std::getline(stream, line)) {
				return L"0";
			}
		}

		line.erase(std::remove(line.begin(), line.end(), L'\r'), line.end());
		const std::wstring trimmed = stripWs(line);
		return trimmed.empty() ? L"0" : trimmed;
	}

	bool isDigitOnly(const std::wstring& text)
	{
		for (const wchar_t ch : text) {
			if (!iswdigit(ch)) {
				return false;
			}
		}

		return !text.empty();
	}

	COLORREF clampColor(int red, int green, int blue)
	{
		red = (std::max)(0, (std::min)(255, red));
		green = (std::max)(0, (std::min)(255, green));
		blue = (std::max)(0, (std::min)(255, blue));
		return RGB(red, green, blue);
	}

	COLORREF brighten(COLORREF color, int amount)
	{
		return clampColor(GetRValue(color) + amount, GetGValue(color) + amount, GetBValue(color) + amount);
	}

	COLORREF darken(COLORREF color, int amount)
	{
		return clampColor(GetRValue(color) - amount, GetGValue(color) - amount, GetBValue(color) - amount);
	}

	COLORREF colorFromBrush(HBRUSH brush)
	{
		LOGBRUSH logBrush = {};
		GetObject(brush, sizeof(logBrush), &logBrush);
		return logBrush.lbColor;
	}

	void fillGradient(HDC hdc, const RECT& rect, COLORREF topColor, COLORREF bottomColor)
	{
		TRIVERTEX vertices[2] = {};
		vertices[0].x = rect.left;
		vertices[0].y = rect.top;
		vertices[0].Red = static_cast<COLOR16>(GetRValue(topColor) << 8);
		vertices[0].Green = static_cast<COLOR16>(GetGValue(topColor) << 8);
		vertices[0].Blue = static_cast<COLOR16>(GetBValue(topColor) << 8);
		vertices[0].Alpha = 0xFF00;

		vertices[1].x = rect.right;
		vertices[1].y = rect.bottom;
		vertices[1].Red = static_cast<COLOR16>(GetRValue(bottomColor) << 8);
		vertices[1].Green = static_cast<COLOR16>(GetGValue(bottomColor) << 8);
		vertices[1].Blue = static_cast<COLOR16>(GetBValue(bottomColor) << 8);
		vertices[1].Alpha = 0xFF00;

		GRADIENT_RECT gradientRect = { 0, 1 };
		GradientFill(hdc, vertices, 2, &gradientRect, 1, GRADIENT_FILL_RECT_V);
	}

	void setStopwatchArrowFont(const HWND hControl)
	{
		const HFONT hFont = CreateFont(
			18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
		);

		SendMessage(hControl, WM_SETFONT, (WPARAM)hFont, TRUE);
	}

	void drawRoundedPanel(HDC hdc, const RECT& rect, COLORREF fillColor, COLORREF borderColor, int radius = 18)
	{
		const HBRUSH brush = CreateSolidBrush(fillColor);
		const HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
		const HGDIOBJ oldBrush = SelectObject(hdc, brush);
		const HGDIOBJ oldPen = SelectObject(hdc, pen);

		RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);

		SelectObject(hdc, oldBrush);
		SelectObject(hdc, oldPen);
		DeleteObject(brush);
		DeleteObject(pen);
	}

	void drawScratchMarks(HDC hdc, const RECT& clientRect)
	{
		const HPEN pen = CreatePen(PS_SOLID, 3, kAccent);
		const HPEN glowPen = CreatePen(PS_SOLID, 1, brighten(kAccentSoft, 10));
		const HGDIOBJ oldPen = SelectObject(hdc, pen);

		const int startX = clientRect.right - 118;
		const int startY = 34;

		for (int i = 0; i < 4; ++i)
		{
			MoveToEx(hdc, startX + i * 18, startY + i * 3, nullptr);
			LineTo(hdc, startX + i * 18 + 12, startY + 54 + i * 3);
		}

		SelectObject(hdc, glowPen);
		for (int i = 0; i < 4; ++i)
		{
			MoveToEx(hdc, startX + i * 18 + 3, startY + i * 3 + 4, nullptr);
			LineTo(hdc, startX + i * 18 + 13, startY + 42 + i * 3);
		}

		SelectObject(hdc, oldPen);
		DeleteObject(pen);
		DeleteObject(glowPen);
	}

	Gdiplus::Image* loadPngResource(const int resourceId)
	{
		const HRSRC resourceInfo = FindResourceW(hInstance_, MAKEINTRESOURCEW(resourceId), L"PNG");
		if (resourceInfo == nullptr) {
			return nullptr;
		}

		const DWORD resourceSize = SizeofResource(hInstance_, resourceInfo);
		const HGLOBAL resourceDataHandle = LoadResource(hInstance_, resourceInfo);
		if (resourceDataHandle == nullptr || resourceSize == 0) {
			return nullptr;
		}

		const void* resourceData = LockResource(resourceDataHandle);
		if (resourceData == nullptr) {
			return nullptr;
		}

		const HGLOBAL imageBuffer = GlobalAlloc(GMEM_MOVEABLE, resourceSize);
		if (imageBuffer == nullptr) {
			return nullptr;
		}

		void* imageData = GlobalLock(imageBuffer);
		if (imageData == nullptr)
		{
			GlobalFree(imageBuffer);
			return nullptr;
		}

		CopyMemory(imageData, resourceData, resourceSize);
		GlobalUnlock(imageBuffer);

		IStream* imageStream = nullptr;
		if (CreateStreamOnHGlobal(imageBuffer, TRUE, &imageStream) != S_OK) {
			GlobalFree(imageBuffer);
			return nullptr;
		}

		Gdiplus::Image* image = Gdiplus::Image::FromStream(imageStream);
		imageStream->Release();

		if (image == nullptr || image->GetLastStatus() != Gdiplus::Ok)
		{
			delete image;
			return nullptr;
		}

		return image;
	}

	void drawOverlayImage(HDC hdc, const RECT& clientRect, Gdiplus::Image* image)
	{
		if (image == nullptr || image->GetLastStatus() != Gdiplus::Ok) {
			return;
		}

		const float sourceWidth = static_cast<float>(image->GetWidth());
		const float sourceHeight = static_cast<float>(image->GetHeight());
		if (sourceWidth <= 0.0f || sourceHeight <= 0.0f) {
			return;
		}

		const int availableWidth = (std::max)(static_cast<int>(clientRect.right) - 24, 120);
		const int availableHeight = (std::max)(static_cast<int>(clientRect.bottom) - 70, 120);
		const float maxWidth = static_cast<float>(availableWidth);
		const float maxHeight = static_cast<float>(availableHeight);
		const float scale = (std::min)(maxWidth / sourceWidth, maxHeight / sourceHeight);
		const int drawWidth = static_cast<int>(sourceWidth * scale);
		const int drawHeight = static_cast<int>(sourceHeight * scale);
		const int drawX = (clientRect.right - drawWidth) / 2;
		const int drawY = (clientRect.bottom - drawHeight) / 2 + 10;

		Gdiplus::Graphics graphics(hdc);
		graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
		graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
		graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);

		Gdiplus::ImageAttributes attributes;
		Gdiplus::ColorMatrix matrix = {
			1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.58f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f, 1.0f
		};
		attributes.SetColorMatrix(&matrix, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);

		graphics.DrawImage(
			image,
			Gdiplus::Rect(drawX, drawY, drawWidth, drawHeight),
			0,
			0,
			image->GetWidth(),
			image->GetHeight(),
			Gdiplus::UnitPixel,
			&attributes);
	}

	bool parseDurationTextLocal(const std::wstring& text, int& durationMillis)
	{
		std::wstring cleaned;
		cleaned.reserve(text.size());

		for (const wchar_t ch : text) {
			if (!iswspace(ch)) {
				cleaned.push_back(ch);
			}
		}

		if (cleaned.empty()) {
			return false;
		}

		if (cleaned.find(L':') == std::wstring::npos)
		{
			if (!isDigitOnly(cleaned)) {
				return false;
			}

			const long long totalSeconds = _wtoll(cleaned.c_str());
			if (totalSeconds < 0 || totalSeconds > INT_MAX / 1000) {
				return false;
			}

			durationMillis = static_cast<int>(totalSeconds * 1000);
			return true;
		}

		std::wstringstream stream(cleaned);
		std::wstring token;
		std::vector<int> parts;

		while (std::getline(stream, token, L':'))
		{
			if (!isDigitOnly(token)) {
				return false;
			}

			parts.push_back(_wtoi(token.c_str()));
		}

		if (parts.size() != 2 && parts.size() != 3) {
			return false;
		}

		long long totalSeconds = 0;
		if (parts.size() == 2)
		{
			if (parts[1] > 59) {
				return false;
			}

			totalSeconds = static_cast<long long>(parts[0]) * 60 + parts[1];
		}
		else
		{
			if (parts[1] > 59 || parts[2] > 59) {
				return false;
			}

			totalSeconds =
				static_cast<long long>(parts[0]) * 3600 +
				static_cast<long long>(parts[1]) * 60 +
				parts[2];
		}

		if (totalSeconds < 0 || totalSeconds > INT_MAX / 1000) {
			return false;
		}

		durationMillis = static_cast<int>(totalSeconds * 1000);
		return true;
	}

	std::wstring formatDurationTextLocal(const int durationMillis)
	{
		const int totalSeconds = (std::max)(durationMillis / 1000, 0);
		const int hours = totalSeconds / 3600;
		const int minutes = (totalSeconds % 3600) / 60;
		const int seconds = totalSeconds % 60;

		wchar_t buffer[32] = {};
		if (hours > 0) {
			swprintf_s(buffer, L"%d:%02d:%02d", hours, minutes, seconds);
		}
		else {
			swprintf_s(buffer, L"%02d:%02d", minutes, seconds);
		}

		return buffer;
	}

	int countNonEmptyWheelItems(const std::wstring& text)
	{
		std::wstringstream stream(text);
		std::wstring line;
		int count = 0;

		while (std::getline(stream, line))
		{
			std::wstring trimmed;
			for (const wchar_t ch : line)
			{
				if (ch != L'\r') {
					trimmed.push_back(ch);
				}
			}

			if (!stripWs(trimmed).empty()) {
				++count;
			}
		}

		return count;
	}

	int clampWheelPresetIndexLocal(const int presetIndex)
	{
		return (std::max)(0, (std::min)(kWheelPresetCount - 1, presetIndex));
	}

	int clampStreakPresetIndexLocal(const int presetIndex)
	{
		return (std::max)(0, (std::min)(kStreakPresetCount - 1, presetIndex));
	}

	std::wstring fallbackPresetName(const int presetIndex)
	{
		wchar_t buffer[32] = {};
		swprintf_s(buffer, L"Preset %d", clampWheelPresetIndexLocal(presetIndex) + 1);
		return buffer;
	}

}

const wchar_t* PrefsPanel::lookupKb(const UINT vk)
{
	for (int i = 0; i < kbEntryCount; ++i)
	{
		if (kbEntries[i].code == vk) return kbEntries[i].tag;
	}
	return L"?";
}

const wchar_t* PrefsPanel::lookupPad(const UINT code)
{
	for (int i = 0; i < padEntryCount; ++i)
	{
		if (padEntries[i].code == code) return padEntries[i].tag;
	}
	return L"?";
}

PrefsPanel::~PrefsPanel()
{
	discardThemeResources();
}

void PrefsPanel::createThemeResources()
{
	if (editBrush_ == nullptr) {
		editBrush_ = CreateSolidBrush(kInputFill);
	}
	if (renameBrush_ == nullptr) {
		renameBrush_ = CreateSolidBrush(kPresetRenameFill);
	}

	if (gdiToken_ == 0)
	{
		Gdiplus::GdiplusStartupInput startupInput;
		if (Gdiplus::GdiplusStartup(&gdiToken_, &startupInput, nullptr) != Gdiplus::Ok) {
			gdiToken_ = 0;
		}
	}

}

void PrefsPanel::discardThemeResources()
{
	if (editBrush_ != nullptr) {
		DeleteObject(editBrush_);
		editBrush_ = nullptr;
	}
	if (renameBrush_ != nullptr) {
		DeleteObject(renameBrush_);
		renameBrush_ = nullptr;
	}
	invalidateBgDib();

	if (gdiToken_ != 0) {
		Gdiplus::GdiplusShutdown(gdiToken_);
		gdiToken_ = 0;
	}
}

void PrefsPanel::invalidateBgDib()
{
	if (bgDib_ != nullptr) {
		DeleteObject(bgDib_);
		bgDib_ = nullptr;
	}
	bgDibBits_ = nullptr;
	bgDibW_ = 0;
	bgDibH_ = 0;
}

void PrefsPanel::ensureBgDib(const int w, const int h)
{
	if (bgDib_ != nullptr && bgDibW_ == w && bgDibH_ == h) {
		return;
	}
	invalidateBgDib();

	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = w;
	bmi.bmiHeader.biHeight = -h;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	const HDC hdcWnd = GetDC(nativeWindow_);
	bgDib_ = CreateDIBSection(hdcWnd, &bmi, DIB_RGB_COLORS, &bgDibBits_, nullptr, 0);
	ReleaseDC(nativeWindow_, hdcWnd);

	if (bgDib_ == nullptr) {
		return;
	}

	const HDC hdcMem = CreateCompatibleDC(nullptr);
	const HGDIOBJ oldBmp = SelectObject(hdcMem, bgDib_);

	RECT clientRect = { 0, 0, w, h };
	fillGradient(hdcMem, clientRect, kBgTop, kBgBottom);
	drawScratchMarks(hdcMem, clientRect);

	const RECT tabsShell = { 20, 8, w - 20, 46 };
	drawRoundedPanel(hdcMem, tabsShell, RGB(9, 14, 22), RGB(30, 45, 66), 18);

	if (currentTab == SettingsPage::PageTimers)
	{
		drawRoundedPanel(hdcMem, { 14, 58, w - 14, 208 }, kCardFill, kCardBorder);
		drawRoundedPanel(hdcMem, { 14, 216, w - 14, 363 }, kCardFill, kCardBorder);
		drawRoundedPanel(hdcMem, { 14, 371, w - 14, 585 }, kCardFill, kCardBorder);
		drawRoundedPanel(hdcMem, { 14, 593, w - 14, 737 }, kCardFill, kCardBorder);
	}
	else if (currentTab == SettingsPage::PageWheel)
	{
		drawRoundedPanel(hdcMem, { 14, 58, w - 14, 724 }, kWheelCardFill, kCardBorder);
	}
	else if (currentTab == SettingsPage::PageBuilds)
	{
		drawRoundedPanel(hdcMem, { 18, 62, w - 18, 122 }, kWheelCardFill, kCardBorder);
		drawRoundedPanel(hdcMem, { 18, 138, w - 18, 286 }, kWheelCardFill, kCardBorder);
		drawRoundedPanel(hdcMem, { 18, 302, w - 18, 450 }, kWheelCardFill, kCardBorder);
	}
	else if (currentTab == SettingsPage::PageExtra)
	{
		drawRoundedPanel(hdcMem, { 18, 96, w - 18, 258 }, kWheelCardFill, kCardBorder);
	}
	else if (currentTab == SettingsPage::PageOverlays)
	{
		drawRoundedPanel(hdcMem, { 18, 96, w - 18, 368 }, kWheelCardFill, kCardBorder);
	}

	SelectObject(hdcMem, oldBmp);
	DeleteDC(hdcMem);
	bgDibW_ = w;
	bgDibH_ = h;
}

void PrefsPanel::applyChromeTheme() const
{
	HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
	if (dwm == nullptr) {
		return;
	}

	using DwmSetWindowAttributeProc = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
	const DwmSetWindowAttributeProc setWindowAttribute =
		reinterpret_cast<DwmSetWindowAttributeProc>(GetProcAddress(dwm, "DwmSetWindowAttribute"));

	if (setWindowAttribute != nullptr)
	{
		constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
		BOOL enabled = TRUE;
		setWindowAttribute(nativeWindow_, DWMWA_USE_IMMERSIVE_DARK_MODE, &enabled, sizeof(enabled));
	}

	FreeLibrary(dwm);
}

void PrefsPanel::layoutPanel()
{
	createThemeResources();
	applyChromeTheme();
	cfgApplied_ = false;
	editedConfig = appSettings_;
	wheelPresetSel_ = clampWheelPresetIndexLocal(editedConfig.wheelData.activePreset);
	editedConfig.wheelData.activePreset = wheelPresetSel_;
	editedConfig.wheelData.entriesText = editedConfig.wheelData.presets[wheelPresetSel_].entries;
	streakPresetSel_ = clampStreakPresetIndexLocal(editedConfig.streakData.activePreset);
	editedConfig.streakData.activePreset = streakPresetSel_;
	placeLabels();
	placeButtons();
	placeWheelControls();
	placeBuildControls();
	placeExtraControls();
	placeOverlayControls();
			setActiveTab(SettingsPage::PageTimers);
}

void PrefsPanel::paintWindow() const
{
	PAINTSTRUCT ps;
	const HDC hdc = BeginPaint(nativeWindow_, &ps);

	RECT clientRect;
	GetClientRect(nativeWindow_, &clientRect);
	const int cw = clientRect.right;
	const int ch = clientRect.bottom;

	const_cast<PrefsPanel*>(this)->ensureBgDib(cw, ch);

	if (bgDib_ != nullptr) {
		const HDC hdcMem = CreateCompatibleDC(hdc);
		const HGDIOBJ oldBmp = SelectObject(hdcMem, bgDib_);
		BitBlt(hdc, 0, 0, cw, ch, hdcMem, 0, 0, SRCCOPY);
		SelectObject(hdcMem, oldBmp);
		DeleteDC(hdcMem);
	}

	if (secondaryPagesVisible_) {
		const RECT secondaryShell = { 20, 46, cw - 20, 88 };
		drawRoundedPanel(hdc, secondaryShell, RGB(8, 13, 21), RGB(35, 61, 88), 16);
		const RECT glowLine = { cw - 12, 20, cw - 9, 36 };
		fillGradient(hdc, glowLine, kAccent, kAccentSoft);
	}
	else {
		const RECT hoverRail = { cw - 12, 20, cw - 9, 36 };
		fillGradient(hdc, hoverRail, RGB(21, 80, 122), kAccentSoft);
	}

	EndPaint(nativeWindow_, &ps);
}

void PrefsPanel::placeLabels()
{
	buildTimingPanelText();
}

void PrefsPanel::rememberTimingControls(std::initializer_list<HWND> controls)
{
	for (const HWND control : controls)
	{
		trackTabControl(control, SettingsPage::PageTimers);
	}
}

void PrefsPanel::buildTimingPanelText()
{
	const int widthHotkey = colW_ * 2;
	const int xHotkey = colW_ * ((gridCols_ / 2) + 1);
	const int xHotkeyCon = WIN_PREFS_W - widthHotkey - 38;
	const int colorRowStartY = 639;
	const int colorRowGap = 34;
	const HWND titleLabelTimers = spawnWidget({WC_STATIC, L"Timers", 24, 64, 82, 24});
	hndTimersTitle_ = titleLabelTimers;
	const HWND titleLabelHotkeys = spawnWidget({WC_STATIC, L"Hotkeys", 24, 224, 82, 24});
	const HWND titleLabelOptions = spawnWidget({WC_STATIC, L"Options", 24, 379, 82, 24});
	const HWND titleLabelColors = spawnWidget({WC_STATIC, L"Colors", 24, 601, 82, 24});

	const HWND firstDurationLabel = spawnWidget({WC_STATIC, L"Timer 1", 24, 110, 170, 24});
	const HWND secondDurationLabel = spawnWidget({WC_STATIC, L"Timer 2", 24, 150, 170, 24});
	const HWND hintLabel = spawnWidget({WC_STATIC, L"Use ss, mm:ss or hh:mm:ss", 24, 186, 220, 20});
	const HWND stopwatchUpArrowLabel = spawnWidget({WC_STATIC, L"\u2191", 140, 126, 20, 22, NULL, SS_CENTER});

	const HWND columnLabelKb = spawnWidget({WC_STATIC, L"Keys / Mouse", xHotkey - 16, 258, widthHotkey + 34, 20, NULL, SS_CENTER});
	const HWND columnLabelController = spawnWidget({WC_STATIC, L"Controller", xHotkeyCon - 8, 258, widthHotkey + 18, 20, NULL, SS_CENTER});

	const HWND firstHotkeyLabel = spawnWidget({WC_STATIC, L"Select Timer 1", 24, 292, 170, 24});
	const HWND secondHotkeyLabel = spawnWidget({WC_STATIC, L"Select Timer 2", 24, 332, 170, 24});

	const HWND optionStartLabel = spawnWidget({WC_STATIC, L"Start On/Off", 24, 417, 190, 24});
	const HWND optionTransparentLabel = spawnWidget({WC_STATIC, L"Transparent Background", 24, 457, 190, 24});
	const HWND optionClickLabel = spawnWidget({WC_STATIC, L"Ignore clicks (resets when closed)", 24, 537, 286, 24});

	const HWND baseColorLabel = spawnWidget({WC_STATIC, L"1st Timer", 24, colorRowStartY, 170, 24});
	const HWND focusColorLabel = spawnWidget({WC_STATIC, L"2nd Timer", 24, colorRowStartY + colorRowGap, 170, 24});
	const HWND lastSecondsLabel = spawnWidget({WC_STATIC, L"Last 20 Seconds", 24, colorRowStartY + (colorRowGap * 2), 170, 24});
	const HWND footerLabel = spawnWidget({WC_STATIC, L"\u00A9 VyxenApps 2026", 0, 775, WIN_PREFS_W, 18, NULL, SS_CENTER});

	rememberTimingControls({
		titleLabelTimers, titleLabelHotkeys, titleLabelOptions, titleLabelColors,
		firstDurationLabel, secondDurationLabel, hintLabel, columnLabelKb, columnLabelController,
		stopwatchUpArrowLabel,
		firstHotkeyLabel, secondHotkeyLabel,
		optionStartLabel, optionTransparentLabel, optionClickLabel,
		baseColorLabel, focusColorLabel, lastSecondsLabel
	});

	setFontChildren(nativeWindow_);
	setFontHeader(titleLabelTimers);
	setFontHeader(titleLabelHotkeys);
	setFontHeader(titleLabelOptions);
	setFontHeader(titleLabelColors);
	setStopwatchArrowFont(stopwatchUpArrowLabel);
	applyBrandingFont(hintLabel);
	applyBrandingFont(columnLabelKb);
	applyBrandingFont(columnLabelController);
	applyBrandingFont(footerLabel);
}

void PrefsPanel::placeButtons()
{
	RECT clientRect = {};
	GetClientRect(nativeWindow_, &clientRect);
	const int clientWidth = clientRect.right > 0 ? clientRect.right : WIN_PREFS_W;
	const int widthHotkey = colW_ * 2;
	const int xHotkey = colW_ * ((gridCols_ / 2) + 1);
	const int xHotkeyCon = WIN_PREFS_W - widthHotkey - 38;
	const int xDuration = colW_ * ((gridCols_ / 2) + 1);
	const int xColorButton = 224;
	const int widthDuration = colW_ * 3;
	const int heightDuration = 22;
	const int widthColorButton = 132;
	const int heightColorButton = 18;
	const int colorRowStartY = 641;
	const int colorRowGap = 34;
	const int heightHotkey = 24;
	const int sizeCheckbox = 20;
	const int xCheckbox = WIN_PREFS_W - 64;

	buildSettingsTopTabs(clientWidth);
	buildDurationFields(xDuration, widthDuration, heightDuration, sizeCheckbox);
	buildHotkeyRowsForTiming(xHotkey, xHotkeyCon, widthHotkey, heightHotkey);
	buildTimingModeToggles(xCheckbox, sizeCheckbox);
	buildTimingColorRows(xColorButton, colorRowStartY, widthColorButton, heightColorButton, colorRowGap);
	buildFooterActions(clientWidth);
	setBackdropMenuVisible(false);
}

void PrefsPanel::buildSettingsTopTabs(const int clientWidth)
{
	const int tabGap = 8;
	const int tabX = 28;
	const int tabY = 12;
	const int tabHeight = 28;
	const int tabWidth = (clientWidth - (tabX * 2) - (tabGap * 2)) / 3;
	const int secondaryTabGap = 8;
	const int secondaryTabWidth = tabWidth;
	const int secondaryTabX = (clientWidth - (secondaryTabWidth * 2) - secondaryTabGap) / 2;
	const int secondaryTabY = 54;

	hndTabTimers_ = spawnWidget({WC_BUTTON, L"Timers", tabX, tabY, tabWidth, tabHeight, CMD_PAGE_TIMERS, BS_OWNERDRAW});
	hndTabWheel_ = spawnWidget({WC_BUTTON, L"Roulette", tabX + tabWidth + tabGap, tabY, tabWidth, tabHeight, CMD_PAGE_WHEEL, BS_OWNERDRAW});
	hndTabBuilds_ = spawnWidget({WC_BUTTON, L"Builds", tabX + ((tabWidth + tabGap) * 2), tabY, tabWidth, tabHeight, CMD_PAGE_BUILDS, BS_OWNERDRAW});
	hndTabExtra_ = spawnWidget({WC_BUTTON, L"Streaks", secondaryTabX, secondaryTabY, secondaryTabWidth, tabHeight, CMD_PAGE_EXTRA, BS_OWNERDRAW});
	hndTabOverlays_ = spawnWidget({WC_BUTTON, L"Overlays", secondaryTabX + secondaryTabWidth + secondaryTabGap, secondaryTabY, secondaryTabWidth, tabHeight, CMD_PAGE_OVERLAYS, BS_OWNERDRAW});
	ShowWindow(hndTabExtra_, SW_HIDE);
	ShowWindow(hndTabOverlays_, SW_HIDE);
}

void PrefsPanel::buildDurationFields(const int xDuration, const int widthDuration, const int heightDuration, const int sizeCheckbox)
{
	const int xStopwatchCheckbox = 164;
	const int yStopwatchCheckbox = 126;
	hndTmr1Dur_ = spawnWidget({WC_EDIT, L"", xDuration, 106, widthDuration, heightDuration, CMD_DUR_INPUT_TMR1, WS_BORDER | ES_AUTOHSCROLL | ES_CENTER});
	hndTmr2Dur_ = spawnWidget({WC_EDIT, L"", xDuration, 146, widthDuration, heightDuration, CMD_DUR_INPUT_TMR2, WS_BORDER | ES_AUTOHSCROLL | ES_CENTER});
	const HWND hCbStopwatchMode = spawnWidget({WC_BUTTON, L"", xStopwatchCheckbox, yStopwatchCheckbox, sizeCheckbox, sizeCheckbox, CMD_CHK_COUNTUP, BS_CHECKBOX | BS_OWNERDRAW});
	SendMessage(hndTmr1Dur_, EM_LIMITTEXT, 8, 0);
	SendMessage(hndTmr2Dur_, EM_LIMITTEXT, 8, 0);
	SetWindowText(hndTmr1Dur_, formatDurationTextLocal(editedConfig.timerOneDuration).c_str());
	SetWindowText(hndTmr2Dur_, formatDurationTextLocal(editedConfig.timerTwoDuration).c_str());
	SendMessage(hCbStopwatchMode, BM_SETCHECK, appSettings_.flagCountUp, 0);

	rememberTimingControls({
		hndTmr1Dur_,
		hndTmr2Dur_,
		hCbStopwatchMode
	});
}

void PrefsPanel::buildHotkeyRowsForTiming(const int xHotkey, const int xHotkeyCon, const int widthHotkey, const int heightHotkey)
{
	HWND hotkeys[4] = {};
	hotkeys[0] = spawnWidget({WC_BUTTON, L"", xHotkey, 288, widthHotkey, heightHotkey, CMD_TIMER_ONE, BS_OWNERDRAW});
	hotkeys[1] = spawnWidget({WC_BUTTON, L"", xHotkey, 328, widthHotkey, heightHotkey, CMD_TIMER_TWO, BS_OWNERDRAW});

	hotkeys[2] = spawnWidget({WC_BUTTON, L"", xHotkeyCon, 288, widthHotkey, heightHotkey, CMD_PAD_BIND_TMR1, BS_OWNERDRAW});
	hotkeys[3] = spawnWidget({WC_BUTTON, L"", xHotkeyCon, 328, widthHotkey, heightHotkey, CMD_PAD_BIND_TMR2, BS_OWNERDRAW});

	for (const HWND hCtrl : hotkeys) {
		restoreHotkeyLabel(hCtrl);
		trackTabControl(hCtrl, SettingsPage::PageTimers);
	}
}

void PrefsPanel::buildTimingModeToggles(const int xCheckbox, const int sizeCheckbox)
{
	const HWND hCbStartOnChange = spawnWidget({WC_BUTTON, L"", xCheckbox, 417, sizeCheckbox, sizeCheckbox, CMD_CHK_AUTO_BEGIN, BS_CHECKBOX | BS_OWNERDRAW});
	const HWND hCbTransparentBg = spawnWidget({WC_BUTTON, L"", xCheckbox, 457, sizeCheckbox, sizeCheckbox, CMD_CHK_SEETHROUGH, BS_CHECKBOX | BS_OWNERDRAW});
	const HWND hCbClickthrough = spawnWidget({WC_BUTTON, L"", xCheckbox, 537, sizeCheckbox, sizeCheckbox, CMD_CHK_PASSTHRU, BS_CHECKBOX | BS_OWNERDRAW});
	const HWND hCbTimerImageMode = spawnWidget({WC_BUTTON, L"", xCheckbox, 497, sizeCheckbox, sizeCheckbox, CMD_CHK_TIMER_IMAGE, BS_CHECKBOX | BS_OWNERDRAW});
	const HWND timerImageLabel = spawnWidget({WC_STATIC, L"Timer Image", 24, 497, 190, 24});
	hndTmrImgLbl_ = timerImageLabel;
	hndBackdropBtn_ = spawnWidget({WC_BUTTON, L"", 340, 379, 24, 24, CMD_BG_STYLE_GEAR, BS_OWNERDRAW});
	hndBackdropSurface_ = spawnWidget({WC_STATIC, L"", 236, 403, 134, 108, CMD_BG_STYLE_PNL, SS_OWNERDRAW});
	hndBackdropOpts_[0] = spawnWidget({WC_BUTTON, L"Actual", 246, 411, 114, 28, CMD_BG_STYLE_CUR, BS_OWNERDRAW});
	hndBackdropOpts_[1] = spawnWidget({WC_BUTTON, L"Negro", 246, 443, 114, 28, CMD_BG_STYLE_BLK, BS_OWNERDRAW});
	hndBackdropOpts_[2] = spawnWidget({WC_BUTTON, L"Blanco", 246, 475, 114, 28, CMD_BG_STYLE_WHT, BS_OWNERDRAW});

	SendMessage(hCbStartOnChange, BM_SETCHECK, appSettings_.flagAutoStart, 0);
	SendMessage(hCbTransparentBg, BM_SETCHECK, appSettings_.flagSeeThrough, 0);
	SendMessage(hCbClickthrough, BM_SETCHECK, appSettings_.flagPassThru, 0);
	SendMessage(hCbTimerImageMode, BM_SETCHECK, appSettings_.flagTimerImg, 0);

	rememberTimingControls({
		hCbStartOnChange,
		hCbTransparentBg,
		hCbClickthrough,
		hCbTimerImageMode,
		timerImageLabel,
		hndBackdropBtn_
	});

	applyBrandingFont(timerImageLabel);
}

void PrefsPanel::buildTimingColorRows(const int xColorButton, const int y, const int width, const int height, const int rowGap)
{
	const HWND hTimerColor = spawnWidget({WC_BUTTON, L"", xColorButton, y, width, height, CMD_CLR_TIMER, BS_OWNERDRAW});
	const HWND hSelectedColor = spawnWidget({WC_BUTTON, L"", xColorButton, y + rowGap, width, height, CMD_CLR_SEL_TIMER, BS_OWNERDRAW});
	const HWND hLastSecondsColor = spawnWidget({WC_BUTTON, L"", xColorButton, y + (rowGap * 2), width, height, CMD_CLR_URGENT, BS_OWNERDRAW});

	rememberTimingControls({
		hTimerColor,
		hSelectedColor,
		hLastSecondsColor
	});
}

void PrefsPanel::buildFooterActions(const int clientWidth)
{
	const int actionButtonWidth = 160;
	const int actionButtonGap = 16;
	const int actionButtonY = 796;
	const int actionButtonsX = (clientWidth - ((actionButtonWidth * 2) + actionButtonGap)) / 2;

	spawnWidget({WC_BUTTON, L"OK", actionButtonsX, actionButtonY, actionButtonWidth, 34, CMD_ACCEPT, BS_OWNERDRAW});
	spawnWidget({WC_BUTTON, L"CANCEL", actionButtonsX + actionButtonWidth + actionButtonGap, actionButtonY, actionButtonWidth, 34, CMD_DISMISS, BS_OWNERDRAW});
}

void PrefsPanel::placeWheelControls()
{
	const HWND hTitle = spawnWidget({WC_STATIC, L"Roulette", 24, 64, 120, 24});
	hndWheelTitle_ = hTitle;
	const HWND hSectionsTitle = spawnWidget({WC_STATIC, L"Sections", 24, 104, 140, 24});
	const HWND hPresetTitle = spawnWidget({WC_STATIC, L"Presets", 214, 104, 140, 24});
	const HWND hSectionsHint = spawnWidget({WC_STATIC, L"One name per line. Double click a preset to rename it.", 24, 134, 176, 36});
	const HWND hImageTitle = spawnWidget({WC_STATIC, L"Center Image", 24, 552, 140, 24});
	const HWND hImageHint = spawnWidget({WC_STATIC, L"Use PNG, JPG or BMP for the circular center photo.", 24, 622, 330, 20});

	hndWhlPresets_ = spawnWidget({
		WC_COMBOBOX,
		L"",
		214, 132, 150, 220,
		CMD_WHEEL_PRESET,
		CBS_DROPDOWNLIST | CBS_HASSTRINGS | CBS_OWNERDRAWFIXED | CBS_NOINTEGRALHEIGHT | WS_VSCROLL});
	hndWhlRename_ = spawnWidget({
		WC_EDIT,
		L"",
		214, 132, 124, 24,
		CMD_WHEEL_RENAME,
		WS_BORDER | ES_AUTOHSCROLL | ES_NOHIDESEL});
	COMBOBOXINFO wheelPresetComboInfo = {};
	wheelPresetComboInfo.cbSize = sizeof(COMBOBOXINFO);
	if (GetComboBoxInfo(hndWhlPresets_, &wheelPresetComboInfo) && wheelPresetComboInfo.hwndItem != nullptr) {
		SetWindowSubclass(wheelPresetComboInfo.hwndItem, panelCallback, 2, CMD_WHEEL_PRESET);
	}

	hndWhlItems_ = spawnWidget({
		WC_EDIT,
		editedConfig.wheelData.entriesText.c_str(),
		24, 176, 318, 360,
		CMD_WHEEL_ITEMS,
		WS_BORDER | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL});
	hndWhlScroll_ = spawnWidget({
		WC_STATIC,
		L"",
		346, 176, 18, 360,
		CMD_WHEEL_SCROLLBAR,
		SS_NOTIFY});
	hndWhlImgPath_ = spawnWidget({
		WC_EDIT,
		editedConfig.wheelData.centerImg.c_str(),
		24, 586, 232, 24,
		CMD_WHEEL_IMAGE,
		WS_BORDER | ES_AUTOHSCROLL});

	const HWND hBrowseButton = spawnWidget({WC_BUTTON, L"Browse", 264, 584, 100, 28, CMD_WHEEL_BROWSE, BS_OWNERDRAW});
	const HWND hOpenButton = spawnWidget({WC_BUTTON, L"Open Wheel", 24, 674, 340, 34, CMD_WHEEL_SHOW, BS_OWNERDRAW});

	ShowWindow(hndWhlRename_, SW_HIDE);
	SendMessage(hndWhlItems_, EM_LIMITTEXT, 4096, 0);
	SendMessage(hndWhlImgPath_, EM_LIMITTEXT, 1024, 0);
	SendMessage(hndWhlRename_, EM_LIMITTEXT, 64, 0);

	const HWND wheelControls[] = {
		hTitle, hSectionsTitle, hPresetTitle, hSectionsHint,
		hImageTitle, hImageHint,
		hndWhlPresets_, hndWhlItems_, hndWhlScroll_, hndWhlImgPath_,
		hBrowseButton, hOpenButton
	};

	for (const HWND control : wheelControls) {
		trackTabControl(control, SettingsPage::PageWheel);
	}

	setFontHeader(hTitle);
	setFontHeader(hSectionsTitle);
	setFontHeader(hPresetTitle);
	setFontHeader(hImageTitle);
	applyBrandingFont(hSectionsHint);
	applyBrandingFont(hImageHint);
	refreshWheelPresetList();
	loadWheelPresetIntoInputs(wheelPresetSel_);
	setFontChildren(nativeWindow_);
	setFontHeader(hTitle);
	setFontHeader(hSectionsTitle);
	setFontHeader(hPresetTitle);
	setFontHeader(hImageTitle);
	applyBrandingFont(hSectionsHint);
	applyBrandingFont(hImageHint);
	syncWheelItemsScrollbar();
}

void PrefsPanel::placeBuildControls()
{
	const HWND hTitle = spawnWidget({WC_STATIC, L"Builds", 32, 74, 120, 26});
	hndBuildsTitle_ = hTitle;
	const HWND hBuildsHint = spawnWidget({WC_STATIC, L"Manage SideSurvivor and SideKiller perk builds.", 32, 98, 300, 20});

	const HWND hSideSurvivorTitle = spawnWidget({WC_STATIC, L"SideSurvivor", 42, 152, 160, 26});
	const HWND hSideSurvivorHint = spawnWidget({WC_STATIC, L"Manage saved survivor builds with 4 perks.", 42, 184, 300, 22});
	const HWND hSideSurvivorButton = spawnWidget({WC_BUTTON, L"Open SideSurvivor Builds", 62, 226, 262, 36, CMD_BUILD_SURV, BS_OWNERDRAW});

	const HWND hSideKillerTitle = spawnWidget({WC_STATIC, L"SideKiller", 42, 316, 160, 26});
	const HWND hSideKillerHint = spawnWidget({WC_STATIC, L"Manage saved killer builds with 4 perks.", 42, 348, 300, 22});
	const HWND hSideKillerButton = spawnWidget({WC_BUTTON, L"Open SideKiller Builds", 62, 390, 262, 36, CMD_BUILD_KLR, BS_OWNERDRAW});

	const HWND buildControls[] = {
		hTitle, hBuildsHint, hSideSurvivorTitle, hSideSurvivorHint, hSideSurvivorButton,
		hSideKillerTitle, hSideKillerHint, hSideKillerButton
	};

	for (const HWND control : buildControls) {
		trackTabControl(control, SettingsPage::PageBuilds);
	}

	setFontChildren(nativeWindow_);
	setFontHeader(hTitle);
	setFontHeader(hSideSurvivorTitle);
	setFontHeader(hSideKillerTitle);
	applyBrandingFont(hBuildsHint);
	applyBrandingFont(hSideSurvivorHint);
	applyBrandingFont(hSideKillerHint);
}

void PrefsPanel::placeExtraControls()
{
	const HWND hTitle = spawnWidget({WC_STATIC, L"Streaks", 32, 108, 220, 26});
	const HWND hWinsTitle = spawnWidget({WC_STATIC, L"Wins", 184, 138, 62, 22, NULL, SS_CENTER});
	const HWND hLossTitle = spawnWidget({WC_STATIC, L"Loss", 258, 138, 62, 22, NULL, SS_CENTER});
	const HWND hSideSurvivorLabel = spawnWidget({WC_STATIC, L"SideSurvivor", 32, 166, 130, 24});
	const HWND hSideKillerLabel = spawnWidget({WC_STATIC, L"SideKiller", 32, 206, 130, 24});

	const HWND hSideSurvivorToggle = spawnWidget({WC_BUTTON, L"", 330, 164, 22, 22, CMD_CHK_STREAK_SURV, BS_CHECKBOX | BS_OWNERDRAW});
	const HWND hSideKillerToggle = spawnWidget({WC_BUTTON, L"", 330, 204, 22, 22, CMD_CHK_STREAK_KLR, BS_CHECKBOX | BS_OWNERDRAW});

	hndStkSurvWins_ = spawnWidget({
		WC_EDIT,
		getLineOrZero(editedConfig.streakData.presets[0].entries, 0).c_str(),
		184, 164, 62, 24,
		CMD_STREAK_SURV_WIN,
		WS_BORDER | ES_AUTOHSCROLL | ES_CENTER});
	hndStkSurvLoss_ = spawnWidget({
		WC_EDIT,
		getLineOrZero(editedConfig.streakData.presets[0].entries, 1).c_str(),
		258, 164, 62, 24,
		CMD_STREAK_SURV_LOSS,
		WS_BORDER | ES_AUTOHSCROLL | ES_CENTER});

	hndStkKlrWins_ = spawnWidget({
		WC_EDIT,
		getLineOrZero(editedConfig.streakData.presets[1].entries, 0).c_str(),
		184, 204, 62, 24,
		CMD_STREAK_KLR_WIN,
		WS_BORDER | ES_AUTOHSCROLL | ES_CENTER});
	hndStkKlrLoss_ = spawnWidget({
		WC_EDIT,
		getLineOrZero(editedConfig.streakData.presets[1].entries, 1).c_str(),
		258, 204, 62, 24,
		CMD_STREAK_KLR_LOSS,
		WS_BORDER | ES_AUTOHSCROLL | ES_CENTER});

	SendMessage(hndStkSurvWins_, EM_LIMITTEXT, 12, 0);
	SendMessage(hndStkSurvLoss_, EM_LIMITTEXT, 12, 0);
	SendMessage(hndStkKlrWins_, EM_LIMITTEXT, 12, 0);
	SendMessage(hndStkKlrLoss_, EM_LIMITTEXT, 12, 0);

	const HWND extraControls[] = {
		hTitle, hWinsTitle, hLossTitle, hSideSurvivorLabel, hSideKillerLabel,
		hSideSurvivorToggle, hSideKillerToggle,
		hndStkSurvWins_, hndStkSurvLoss_,
		hndStkKlrWins_, hndStkKlrLoss_
	};

	for (const HWND control : extraControls) {
		trackTabControl(control, SettingsPage::PageExtra);
	}

	setFontChildren(nativeWindow_);
	setFontHeader(hTitle);
	applyBrandingFont(hWinsTitle);
	applyBrandingFont(hLossTitle);
	updateStreakToggleControls();
}

void PrefsPanel::placeOverlayControls()
{
	const HWND hTitle = spawnWidget({WC_STATIC, L"Toxic Chat", 32, 108, 180, 26});
	const HWND hHint = spawnWidget({WC_STATIC, L"Cover the toxic chat area on a 1920x1080 screen.", 32, 144, 310, 34});

	const HWND hHotkeyTitle = spawnWidget({WC_STATIC, L"Chat Overlay", 32, 198, 160, 24});
	hndOtxKey_ = spawnWidget({WC_BUTTON, L"", 218, 194, 112, 28, CMD_OVERLAY_KEY, BS_OWNERDRAW});

	const HWND hTimedLabel = spawnWidget({WC_STATIC, L"Use timer", 32, 240, 160, 24});
	hndOtxTimed_ = spawnWidget({WC_BUTTON, L"", 176, 240, 20, 20, CMD_CHK_OVERLAY_TIMED, BS_CHECKBOX | BS_OWNERDRAW});
	hndOtxDur_ = spawnWidget({
		WC_EDIT,
		formatDurationTextLocal(editedConfig.notifyProfile.toxicChatDurationMs).c_str(),
		218, 236, 112, 24,
		CMD_OVERLAY_DUR,
		WS_BORDER | ES_AUTOHSCROLL | ES_CENTER});

	const HWND hImageTitle = spawnWidget({WC_STATIC, L"Background Image", 32, 284, 160, 24});
	hndOtxImgPath_ = spawnWidget({
		WC_EDIT,
		editedConfig.notifyProfile.toxicChatImg.c_str(),
		32, 316, 210, 24,
		CMD_OVERLAY_IMG,
		WS_BORDER | ES_AUTOHSCROLL});
	const HWND hBrowseButton = spawnWidget({WC_BUTTON, L"Browse", 252, 314, 78, 28, CMD_OVERLAY_BROWSE, BS_OWNERDRAW});

	SendMessage(hndOtxDur_, EM_LIMITTEXT, 8, 0);
	SendMessage(hndOtxImgPath_, EM_LIMITTEXT, 1024, 0);
	SendMessage(hndOtxTimed_, BM_SETCHECK, editedConfig.notifyProfile.toxicChatTimed ? BST_CHECKED : BST_UNCHECKED, 0);
	restoreHotkeyLabel(hndOtxKey_);

	const HWND overlayControls[] = {
		hTitle, hHint, hHotkeyTitle, hndOtxKey_,
		hTimedLabel, hndOtxTimed_, hndOtxDur_,
		hImageTitle, hndOtxImgPath_, hBrowseButton
	};

	for (const HWND control : overlayControls) {
		trackTabControl(control, SettingsPage::PageOverlays);
	}

	setFontChildren(nativeWindow_);
	setFontHeader(hTitle);
	setFontHeader(hImageTitle);
	applyBrandingFont(hHint);
	applyBrandingFont(hHotkeyTitle);
	updateOverlayToggleControls();
}

void PrefsPanel::trackTabControl(const HWND control, const SettingsPage tab)
{
	if (control == nullptr) {
		return;
	}

	if (tab == SettingsPage::PageTimers) {
		timerCtrls_.push_back(control);
	}
	else if (tab == SettingsPage::PageWheel) {
		wheelCtrls_.push_back(control);
	}
	else if (tab == SettingsPage::PageBuilds) {
		buildCtrls_.push_back(control);
	}
	else if (tab == SettingsPage::PageExtra) {
		extraCtrls_.push_back(control);
	}
	else {
		overlayCtrls_.push_back(control);
	}
}

void PrefsPanel::setActiveTab(const SettingsPage tab)
{
	commitWheelPresetRename(true);
	endWheelItemsScrollDrag();
	endStreakItemsScrollDrag();

	if (activeHotkeyControl_ != nullptr)
	{
		restoreHotkeyLabel(activeHotkeyControl_);
		activeHotkeyControl_ = nullptr;
	}

	currentTab = tab;
	backdropOpen_ = false;

	for (const HWND control : timerCtrls_) {
		ShowWindow(control, tab == SettingsPage::PageTimers ? SW_SHOW : SW_HIDE);
	}

	for (const HWND control : wheelCtrls_) {
		ShowWindow(control, tab == SettingsPage::PageWheel ? SW_SHOW : SW_HIDE);
	}

	for (const HWND control : buildCtrls_) {
		ShowWindow(control, tab == SettingsPage::PageBuilds ? SW_SHOW : SW_HIDE);
	}

	for (const HWND control : extraCtrls_) {
		ShowWindow(control, tab == SettingsPage::PageExtra ? SW_SHOW : SW_HIDE);
	}

	for (const HWND control : overlayCtrls_) {
		ShowWindow(control, tab == SettingsPage::PageOverlays ? SW_SHOW : SW_HIDE);
	}

	setBackdropMenuVisible(false);
	setSecondaryPagesVisible(tab == SettingsPage::PageExtra || tab == SettingsPage::PageOverlays);

	invalidateBgDib();
	RedrawWindow(nativeWindow_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
}

void PrefsPanel::setSecondaryPagesVisible(const bool isOpen)
{
	const bool shouldOpen = isOpen || currentTab == SettingsPage::PageExtra || currentTab == SettingsPage::PageOverlays;
	if (secondaryPagesVisible_ == shouldOpen)
	{
		return;
	}

	secondaryPagesVisible_ = shouldOpen;
	if (hndTabExtra_ != nullptr)
	{
		ShowWindow(hndTabExtra_, secondaryPagesVisible_ ? SW_SHOW : SW_HIDE);
		if (secondaryPagesVisible_) {
			SetWindowPos(hndTabExtra_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		}
	}
	if (hndTabOverlays_ != nullptr)
	{
		ShowWindow(hndTabOverlays_, secondaryPagesVisible_ ? SW_SHOW : SW_HIDE);
		if (secondaryPagesVisible_) {
			SetWindowPos(hndTabOverlays_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		}
	}

	updateHeaderTitleVisibility();
	redrawTabStrip();
}

void PrefsPanel::updateHeaderTitleVisibility() const
{
	if (hndTimersTitle_ != nullptr && currentTab == SettingsPage::PageTimers) {
		ShowWindow(hndTimersTitle_, secondaryPagesVisible_ ? SW_HIDE : SW_SHOW);
	}

	if (hndWheelTitle_ != nullptr && currentTab == SettingsPage::PageWheel) {
		ShowWindow(hndWheelTitle_, secondaryPagesVisible_ ? SW_HIDE : SW_SHOW);
	}

	if (hndBuildsTitle_ != nullptr && currentTab == SettingsPage::PageBuilds) {
		ShowWindow(hndBuildsTitle_, secondaryPagesVisible_ ? SW_HIDE : SW_SHOW);
	}
}

void PrefsPanel::redrawTabStrip() const
{
	RECT dirtyRect = { 16, 8, WIN_PREFS_W - 4, 122 };
	InvalidateRect(nativeWindow_, &dirtyRect, FALSE);

	if (hndTabExtra_ != nullptr && secondaryPagesVisible_) {
		SetWindowPos(hndTabExtra_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		RedrawWindow(hndTabExtra_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
	}
	if (hndTabOverlays_ != nullptr && secondaryPagesVisible_) {
		SetWindowPos(hndTabOverlays_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		RedrawWindow(hndTabOverlays_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
	}
}

bool PrefsPanel::isPointInsideSecondaryTabs(const POINT point) const
{
	const RECT hoverRail = { WIN_PREFS_W - 24, 8, WIN_PREFS_W - 4, 48 };
	if (PtInRect(&hoverRail, point)) {
		return true;
	}

	if (secondaryPagesVisible_)
	{
		const RECT secondaryShell = { 20, 46, WIN_PREFS_W - 20, 90 };
		if (PtInRect(&secondaryShell, point)) {
			return true;
		}
	}

	if (hndTabExtra_ != nullptr && IsWindowVisible(hndTabExtra_))
	{
		RECT tabRect = {};
		GetWindowRect(hndTabExtra_, &tabRect);
		MapWindowPoints(HWND_DESKTOP, nativeWindow_, reinterpret_cast<LPPOINT>(&tabRect), 2);
		if (PtInRect(&tabRect, point)) {
			return true;
		}
	}

	if (hndTabOverlays_ != nullptr && IsWindowVisible(hndTabOverlays_))
	{
		RECT tabRect = {};
		GetWindowRect(hndTabOverlays_, &tabRect);
		MapWindowPoints(HWND_DESKTOP, nativeWindow_, reinterpret_cast<LPPOINT>(&tabRect), 2);
		if (PtInRect(&tabRect, point)) {
			return true;
		}
	}

	return false;
}

void PrefsPanel::updateSecondaryTabHover(const POINT point)
{
	if (isPointInsideSecondaryTabs(point)) {
		setSecondaryPagesVisible(true);
		return;
	}

	if (secondaryPagesVisible_ && currentTab != SettingsPage::PageExtra && currentTab != SettingsPage::PageOverlays && point.y > 92) {
		setSecondaryPagesVisible(false);
	}
}

std::wstring PrefsPanel::getControlText(const HWND control) const
{
	if (control == nullptr) {
		return L"";
	}

	const int textLength = GetWindowTextLengthW(control);
	std::wstring text(textLength + 1, L'\0');
	GetWindowTextW(control, &text[0], textLength + 1);
	text.resize(textLength);
	return text;
}

void PrefsPanel::syncWheelPresetFromInputs()
{
	if (hndWhlItems_ == nullptr || wheelPresetSel_ < 0 || wheelPresetSel_ >= static_cast<int>(editedConfig.wheelData.presets.size())) {
		return;
	}

	editedConfig.wheelData.presets[wheelPresetSel_].entries = getControlText(hndWhlItems_);
	editedConfig.wheelData.entriesText = editedConfig.wheelData.presets[wheelPresetSel_].entries;
	editedConfig.wheelData.activePreset = wheelPresetSel_;
}

void PrefsPanel::loadWheelPresetIntoInputs(const int presetIndex)
{
	const int clampedPresetIndex = clampWheelPresetIndexLocal(presetIndex);
	wheelPresetSel_ = clampedPresetIndex;
	editedConfig.wheelData.activePreset = clampedPresetIndex;
	editedConfig.wheelData.entriesText = editedConfig.wheelData.presets[clampedPresetIndex].entries;

	if (hndWhlItems_ != nullptr) {
		SetWindowTextW(hndWhlItems_, editedConfig.wheelData.entriesText.c_str());
	}

	if (hndWhlPresets_ != nullptr) {
		SendMessageW(hndWhlPresets_, CB_SETCURSEL, clampedPresetIndex, 0);
	}

	syncWheelItemsScrollbar();
}

void PrefsPanel::refreshWheelPresetList()
{
	if (hndWhlPresets_ == nullptr) {
		return;
	}

	SendMessageW(hndWhlPresets_, CB_RESETCONTENT, 0, 0);
	for (int index = 0; index < static_cast<int>(editedConfig.wheelData.presets.size()); ++index)
	{
		const std::wstring trimmedName = stripWs(editedConfig.wheelData.presets[index].label);
		if (trimmedName.empty()) {
			editedConfig.wheelData.presets[index].label = fallbackPresetName(index);
		}

		SendMessageW(hndWhlPresets_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(editedConfig.wheelData.presets[index].label.c_str()));
	}

	SendMessageW(hndWhlPresets_, CB_SETCURSEL, clampWheelPresetIndexLocal(wheelPresetSel_), 0);
}

void PrefsPanel::selectWheelPreset(const int presetIndex)
{
	const int clampedPresetIndex = clampWheelPresetIndexLocal(presetIndex);
	if (clampedPresetIndex == wheelPresetSel_) {
		return;
	}

	commitWheelPresetRename(true);
	syncWheelPresetFromInputs();
	loadWheelPresetIntoInputs(clampedPresetIndex);
}

void PrefsPanel::beginWheelPresetRename(int presetIndex)
{
	if (hndWhlPresets_ == nullptr || hndWhlRename_ == nullptr) {
		return;
	}

	const int currentSelection = static_cast<int>(SendMessageW(hndWhlPresets_, CB_GETCURSEL, 0, 0));
	const int targetPresetIndex = presetIndex >= 0 ? clampWheelPresetIndexLocal(presetIndex) : clampWheelPresetIndexLocal(currentSelection);
	if (currentSelection == CB_ERR && presetIndex < 0) {
		return;
	}

	commitWheelPresetRename(true);
	SendMessageW(hndWhlPresets_, CB_SETCURSEL, targetPresetIndex, 0);
	whlRenameIdx_ = targetPresetIndex;

	COMBOBOXINFO comboInfo = {};
	comboInfo.cbSize = sizeof(COMBOBOXINFO);
	if (!GetComboBoxInfo(hndWhlPresets_, &comboInfo))
	{
		whlRenameIdx_ = -1;
		return;
	}

	RECT itemRect = comboInfo.rcItem;
	MapWindowPoints(nullptr, nativeWindow_, reinterpret_cast<LPPOINT>(&itemRect), 2);
	SetWindowTextW(hndWhlRename_, editedConfig.wheelData.presets[targetPresetIndex].label.c_str());
	SetWindowPos(
		hndWhlRename_,
		HWND_TOP,
		itemRect.left + 1,
		itemRect.top + 1,
		(itemRect.right - itemRect.left) - 2,
		(itemRect.bottom - itemRect.top) - 2,
		SWP_SHOWWINDOW);
	SetFocus(hndWhlRename_);
	SendMessageW(hndWhlRename_, EM_SETSEL, 0, -1);
	SendMessageW(hndWhlRename_, EM_SCROLLCARET, 0, 0);
	RedrawWindow(hndWhlRename_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
}

void PrefsPanel::commitWheelPresetRename(const bool applyChanges)
{
	if (whlRenameIdx_ < 0 || hndWhlRename_ == nullptr) {
		return;
	}

	const int presetIndex = whlRenameIdx_;
	whlRenameIdx_ = -1;

	if (applyChanges)
	{
		std::wstring presetName = stripWs(getControlText(hndWhlRename_));
		if (presetName.empty()) {
			presetName = fallbackPresetName(presetIndex);
		}

		editedConfig.wheelData.presets[presetIndex].label = presetName;
	}

	ShowWindow(hndWhlRename_, SW_HIDE);
	refreshWheelPresetList();
}

bool PrefsPanel::applyWheelInputs(const bool requireReadyForOpen)
{
	commitWheelPresetRename(true);
	syncWheelPresetFromInputs();
	editedConfig.wheelData.centerImg = stripWs(getControlText(hndWhlImgPath_));

	const int itemCount = countNonEmptyWheelItems(editedConfig.wheelData.entriesText);
	if (requireReadyForOpen && itemCount < 2)
	{
		MessageBox(nativeWindow_, L"Add at least two names to open the wheel.", L"Wheel", MB_OK | MB_ICONWARNING);
		return false;
	}

	return true;
}

bool PrefsPanel::applyOverlayInputs(const bool validateDuration)
{
	editedConfig.notifyProfile.toxicChatImg = stripWs(getControlText(hndOtxImgPath_));

	int durationMillis = editedConfig.notifyProfile.toxicChatDurationMs;
	if (!parseDurationTextLocal(getControlText(hndOtxDur_), durationMillis) || durationMillis <= 0)
	{
		if (validateDuration)
		{
			MessageBox(nativeWindow_, L"Use ss, mm:ss or hh:mm:ss for the Chat Toxic duration.", L"Invalid time", MB_OK | MB_ICONWARNING);
			SetWindowTextW(hndOtxDur_, formatDurationTextLocal(editedConfig.notifyProfile.toxicChatDurationMs).c_str());
			return false;
		}
	}
	else
	{
		editedConfig.notifyProfile.toxicChatDurationMs = durationMillis;
	}

	return true;
}

void PrefsPanel::browseForWheelImage()
{
	wchar_t filePath[MAX_PATH] = {};
	OPENFILENAMEW dialog = {};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = nativeWindow_;
	dialog.lpstrFile = filePath;
	dialog.nMaxFile = MAX_PATH;
	dialog.lpstrFilter = L"Images (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0All Files (*.*)\0*.*\0";
	dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

	if (GetOpenFileNameW(&dialog))
	{
		SetWindowTextW(hndWhlImgPath_, filePath);
		editedConfig.wheelData.centerImg = filePath;
	}
}

void PrefsPanel::browseForOverlayImage()
{
	wchar_t filePath[MAX_PATH] = {};
	OPENFILENAMEW dialog = {};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = nativeWindow_;
	dialog.lpstrFile = filePath;
	dialog.nMaxFile = MAX_PATH;
	dialog.lpstrFilter = L"Images (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0All Files (*.*)\0*.*\0";
	dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

	if (GetOpenFileNameW(&dialog))
	{
		SetWindowTextW(hndOtxImgPath_, filePath);
		editedConfig.notifyProfile.toxicChatImg = filePath;
		if (chatOverlay != nullptr) {
			chatOverlay->applyConfig(editedConfig.notifyProfile);
		}
	}
}

HWND PrefsPanel::spawnWidget(const WidgetCfg& cfg) const
{
	const HWND ctrlHwnd = CreateWindowEx(
		0, cfg.cls, cfg.txt,
		WS_VISIBLE | WS_CHILDWINDOW | cfg.sty,
		cfg.x, cfg.y, cfg.w, cfg.h,
		nativeWindow_,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(cfg.id)),
		nullptr,
		nullptr);

	SetWindowSubclass(ctrlHwnd, panelCallback, 1, 0);
	return ctrlHwnd;
}

int PrefsPanel::getWheelItemsVisibleLineCount() const
{
	if (hndWhlItems_ == nullptr) {
		return 1;
	}

	RECT clientRect = {};
	GetClientRect(hndWhlItems_, &clientRect);

	const HDC hdc = GetDC(hndWhlItems_);
	const HFONT hFont = reinterpret_cast<HFONT>(SendMessageW(hndWhlItems_, WM_GETFONT, 0, 0));
	const HGDIOBJ oldFont = hFont != nullptr ? SelectObject(hdc, hFont) : nullptr;
	TEXTMETRICW textMetrics = {};
	GetTextMetricsW(hdc, &textMetrics);
	if (oldFont != nullptr) {
		SelectObject(hdc, oldFont);
	}
	ReleaseDC(hndWhlItems_, hdc);

	const int lineHeight = (std::max)(1, static_cast<int>(textMetrics.tmHeight + textMetrics.tmExternalLeading));
	const int clientHeight = static_cast<int>(clientRect.bottom - clientRect.top);
	return (std::max)(1, clientHeight / lineHeight);
}

int PrefsPanel::getWheelItemsMaxTopLine() const
{
	if (hndWhlItems_ == nullptr) {
		return 0;
	}

	const int totalLines = (std::max)(1, static_cast<int>(SendMessageW(hndWhlItems_, EM_GETLINECOUNT, 0, 0)));
	return (std::max)(0, totalLines - getWheelItemsVisibleLineCount());
}

RECT PrefsPanel::getWheelItemsScrollThumbRect() const
{
	RECT clientRect = {};
	if (hndWhlScroll_ == nullptr) {
		return clientRect;
	}

	GetClientRect(hndWhlScroll_, &clientRect);

	const int totalLines = hndWhlItems_ == nullptr ? 1 : (std::max)(1, static_cast<int>(SendMessageW(hndWhlItems_, EM_GETLINECOUNT, 0, 0)));
	const int visibleLines = getWheelItemsVisibleLineCount();
	const int maxTopLine = (std::max)(0, totalLines - visibleLines);
	const int firstVisibleLine = hndWhlItems_ == nullptr ? 0 : (std::max)(0, static_cast<int>(SendMessageW(hndWhlItems_, EM_GETFIRSTVISIBLELINE, 0, 0)));

	const int trackPadding = 3;
	const int thumbPadding = 2;
	const int trackTop = static_cast<int>(clientRect.top) + trackPadding;
	const int trackBottom = static_cast<int>(clientRect.bottom) - trackPadding;
	const int trackHeight = (std::max)(1, trackBottom - trackTop);
	const int thumbHeight = (std::min)(trackHeight, (std::max)(36, MulDiv(trackHeight, visibleLines, totalLines)));
	const int thumbTravel = (std::max)(0, trackHeight - thumbHeight);
	const int clampedFirstVisible = (std::min)(firstVisibleLine, maxTopLine);
	const int thumbTop = trackTop + (maxTopLine == 0 ? 0 : MulDiv(clampedFirstVisible, thumbTravel, maxTopLine));

	RECT thumbRect = {
		clientRect.left + thumbPadding,
		thumbTop,
		clientRect.right - thumbPadding,
		thumbTop + thumbHeight
	};

	if (thumbRect.bottom > trackBottom) {
		const int overlap = thumbRect.bottom - trackBottom;
		thumbRect.top -= overlap;
		thumbRect.bottom -= overlap;
	}

	return thumbRect;
}

void PrefsPanel::drawWheelItemsScrollbar(const HDC hdc) const
{
	if (hndWhlScroll_ == nullptr) {
		return;
	}

	RECT clientRect = {};
	GetClientRect(hndWhlScroll_, &clientRect);
	drawRoundedPanel(hdc, clientRect, kScrollTrackFill, brighten(kCardBorder, 6), 10);

	RECT innerRect = clientRect;
	InflateRect(&innerRect, -1, -1);
	drawRoundedPanel(hdc, innerRect, kInputFill, darken(kCardBorder, 6), 9);

	RECT thumbRect = getWheelItemsScrollThumbRect();
	const bool hasOverflow = getWheelItemsMaxTopLine() > 0;
	const COLORREF thumbFill = hasOverflow ? kScrollThumbFill : kScrollThumbDormant;
	const COLORREF thumbBorder = hasOverflow ? kScrollThumbBorder : brighten(kCardBorder, 10);

	drawRoundedPanel(hdc, thumbRect, thumbFill, thumbBorder, 8);

	RECT gripRect = thumbRect;
	InflateRect(&gripRect, -4, -10);
	if (gripRect.bottom - gripRect.top >= 10)
	{
		const HPEN gripPen = CreatePen(PS_SOLID, 1, hasOverflow ? RGB(11, 24, 36) : kTextMuted);
		const HGDIOBJ oldPen = SelectObject(hdc, gripPen);
		const int midY = (gripRect.top + gripRect.bottom) / 2;
		MoveToEx(hdc, gripRect.left, midY - 4, nullptr);
		LineTo(hdc, gripRect.right, midY - 4);
		MoveToEx(hdc, gripRect.left, midY, nullptr);
		LineTo(hdc, gripRect.right, midY);
		MoveToEx(hdc, gripRect.left, midY + 4, nullptr);
		LineTo(hdc, gripRect.right, midY + 4);
		SelectObject(hdc, oldPen);
		DeleteObject(gripPen);
	}
}

void PrefsPanel::syncWheelItemsScrollbar()
{
	if (hndWhlScroll_ == nullptr) {
		return;
	}

	InvalidateRect(hndWhlScroll_, nullptr, FALSE);
	UpdateWindow(hndWhlScroll_);
}

void PrefsPanel::setWheelItemsTopLine(const int topLine)
{
	if (hndWhlItems_ == nullptr) {
		return;
	}

	const int maxTopLine = getWheelItemsMaxTopLine();
	const int clampedTopLine = (std::max)(0, (std::min)(maxTopLine, topLine));
	const int firstVisibleLine = static_cast<int>(SendMessageW(hndWhlItems_, EM_GETFIRSTVISIBLELINE, 0, 0));
	const int lineDelta = clampedTopLine - firstVisibleLine;
	if (lineDelta != 0) {
		SendMessageW(hndWhlItems_, EM_LINESCROLL, 0, lineDelta);
	}

	syncWheelItemsScrollbar();
}

void PrefsPanel::beginWheelItemsScrollDrag(const int mouseY)
{
	if (hndWhlScroll_ == nullptr) {
		return;
	}

	RECT clientRect = {};
	GetClientRect(hndWhlScroll_, &clientRect);
	const RECT thumbRect = getWheelItemsScrollThumbRect();
	if (mouseY >= thumbRect.top && mouseY <= thumbRect.bottom)
	{
		whlScrollDrag_ = true;
		whlScrollOff_ = mouseY - thumbRect.top;
		SetCapture(hndWhlScroll_);
		return;
	}

	const int trackPadding = 3;
	const int thumbHeight = thumbRect.bottom - thumbRect.top;
	const int trackTop = static_cast<int>(clientRect.top) + trackPadding;
	const int trackBottom = (std::max)(trackTop + 1, static_cast<int>(clientRect.bottom) - trackPadding);
	const int thumbTravel = (std::max)(1, (trackBottom - trackTop) - thumbHeight);
	const int maxTopLine = getWheelItemsMaxTopLine();
	const int targetTopLine = maxTopLine == 0
		? 0
		: MulDiv((std::max)(0, mouseY - trackTop - (thumbHeight / 2)), maxTopLine, thumbTravel);
	setWheelItemsTopLine(targetTopLine);
}

void PrefsPanel::updateWheelItemsScrollDrag(const int mouseY)
{
	if (!whlScrollDrag_ || hndWhlScroll_ == nullptr) {
		return;
	}

	RECT clientRect = {};
	GetClientRect(hndWhlScroll_, &clientRect);
	const RECT thumbRect = getWheelItemsScrollThumbRect();
	const int trackPadding = 3;
	const int trackTop = static_cast<int>(clientRect.top) + trackPadding;
	const int trackBottom = static_cast<int>(clientRect.bottom) - trackPadding;
	const int thumbHeight = thumbRect.bottom - thumbRect.top;
	const int thumbTravel = (std::max)(1, (trackBottom - trackTop) - thumbHeight);
	const int maxTopLine = getWheelItemsMaxTopLine();
	const int thumbTop = (std::max)(trackTop, (std::min)(trackBottom - thumbHeight, mouseY - whlScrollOff_));
	const int targetTopLine = maxTopLine == 0
		? 0
		: MulDiv(thumbTop - trackTop, maxTopLine, thumbTravel);
	setWheelItemsTopLine(targetTopLine);
}

void PrefsPanel::endWheelItemsScrollDrag()
{
	if (!whlScrollDrag_) {
		return;
	}

	whlScrollDrag_ = false;
	if (GetCapture() == hndWhlScroll_) {
		ReleaseCapture();
	}
}

void PrefsPanel::syncStreakPresetFromInputs()
{
	const auto makeLines = [this](const HWND winsEdit, const HWND lossEdit) -> std::wstring
	{
		std::wstring wins = stripWs(getControlText(winsEdit));
		std::wstring loss = stripWs(getControlText(lossEdit));
		if (wins.empty()) {
			wins = L"0";
		}
		if (loss.empty()) {
			loss = L"0";
		}

		return wins + L"\r\n" + loss;
	};

	if (hndStkSurvWins_ != nullptr && hndStkSurvLoss_ != nullptr)
	{
		editedConfig.streakData.presets[0].entries = makeLines(hndStkSurvWins_, hndStkSurvLoss_);
	}
	if (hndStkKlrWins_ != nullptr && hndStkKlrLoss_ != nullptr)
	{
		editedConfig.streakData.presets[1].entries = makeLines(hndStkKlrWins_, hndStkKlrLoss_);
	}

	streakPresetSel_ = clampStreakPresetIndexLocal(streakPresetSel_);
	editedConfig.streakData.activePreset = streakPresetSel_;
}

void PrefsPanel::loadStreakPresetIntoInputs(const int presetIndex)
{
	streakPresetSel_ = clampStreakPresetIndexLocal(presetIndex);
	editedConfig.streakData.activePreset = streakPresetSel_;

	if (hndStkSurvWins_ != nullptr) {
		SetWindowTextW(hndStkSurvWins_, getLineOrZero(editedConfig.streakData.presets[0].entries, 0).c_str());
	}
	if (hndStkSurvLoss_ != nullptr) {
		SetWindowTextW(hndStkSurvLoss_, getLineOrZero(editedConfig.streakData.presets[0].entries, 1).c_str());
	}
	if (hndStkKlrWins_ != nullptr) {
		SetWindowTextW(hndStkKlrWins_, getLineOrZero(editedConfig.streakData.presets[1].entries, 0).c_str());
	}
	if (hndStkKlrLoss_ != nullptr) {
		SetWindowTextW(hndStkKlrLoss_, getLineOrZero(editedConfig.streakData.presets[1].entries, 1).c_str());
	}
}

void PrefsPanel::refreshStreakPresetList()
{
	if (hndStkPresets_ == nullptr) {
		return;
	}

	SendMessageW(hndStkPresets_, CB_RESETCONTENT, 0, 0);
	for (int index = 0; index < kStreakPresetCount; ++index)
	{
		SendMessageW(hndStkPresets_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(editedConfig.streakData.presets[index].label.c_str()));
	}

	SendMessageW(hndStkPresets_, CB_SETCURSEL, clampStreakPresetIndexLocal(streakPresetSel_), 0);
}

void PrefsPanel::selectStreakPreset(const int presetIndex)
{
	syncStreakPresetFromInputs();
	streakPresetSel_ = clampStreakPresetIndexLocal(presetIndex);
	editedConfig.streakData.activePreset = streakPresetSel_;
}

int PrefsPanel::getStreakItemsVisibleLineCount() const
{
	if (hndStkItems_ == nullptr) {
		return 1;
	}

	RECT clientRect = {};
	GetClientRect(hndStkItems_, &clientRect);

	const HDC hdc = GetDC(hndStkItems_);
	const HFONT hFont = reinterpret_cast<HFONT>(SendMessageW(hndStkItems_, WM_GETFONT, 0, 0));
	const HGDIOBJ oldFont = hFont != nullptr ? SelectObject(hdc, hFont) : nullptr;
	TEXTMETRICW textMetrics = {};
	GetTextMetricsW(hdc, &textMetrics);
	if (oldFont != nullptr) {
		SelectObject(hdc, oldFont);
	}
	ReleaseDC(hndStkItems_, hdc);

	const int lineHeight = (std::max)(1, static_cast<int>(textMetrics.tmHeight + textMetrics.tmExternalLeading));
	const int clientHeight = static_cast<int>(clientRect.bottom - clientRect.top);
	return (std::max)(1, clientHeight / lineHeight);
}

int PrefsPanel::getStreakItemsMaxTopLine() const
{
	if (hndStkItems_ == nullptr) {
		return 0;
	}

	const int totalLines = (std::max)(1, static_cast<int>(SendMessageW(hndStkItems_, EM_GETLINECOUNT, 0, 0)));
	return (std::max)(0, totalLines - getStreakItemsVisibleLineCount());
}

RECT PrefsPanel::getStreakItemsScrollThumbRect() const
{
	RECT clientRect = {};
	if (hndStkScroll_ == nullptr) {
		return clientRect;
	}

	GetClientRect(hndStkScroll_, &clientRect);

	const int totalLines = hndStkItems_ == nullptr ? 1 : (std::max)(1, static_cast<int>(SendMessageW(hndStkItems_, EM_GETLINECOUNT, 0, 0)));
	const int visibleLines = getStreakItemsVisibleLineCount();
	const int maxTopLine = (std::max)(0, totalLines - visibleLines);
	const int firstVisibleLine = hndStkItems_ == nullptr ? 0 : (std::max)(0, static_cast<int>(SendMessageW(hndStkItems_, EM_GETFIRSTVISIBLELINE, 0, 0)));

	const int trackPadding = 3;
	const int thumbPadding = 2;
	const int trackTop = static_cast<int>(clientRect.top) + trackPadding;
	const int trackBottom = static_cast<int>(clientRect.bottom) - trackPadding;
	const int trackHeight = (std::max)(1, trackBottom - trackTop);
	const int thumbHeight = (std::min)(trackHeight, (std::max)(36, MulDiv(trackHeight, visibleLines, totalLines)));
	const int thumbTravel = (std::max)(0, trackHeight - thumbHeight);
	const int clampedFirstVisible = (std::min)(firstVisibleLine, maxTopLine);
	const int thumbTop = trackTop + (maxTopLine == 0 ? 0 : MulDiv(clampedFirstVisible, thumbTravel, maxTopLine));

	RECT thumbRect = {
		clientRect.left + thumbPadding,
		thumbTop,
		clientRect.right - thumbPadding,
		thumbTop + thumbHeight
	};

	if (thumbRect.bottom > trackBottom) {
		const int overlap = thumbRect.bottom - trackBottom;
		thumbRect.top -= overlap;
		thumbRect.bottom -= overlap;
	}

	return thumbRect;
}

void PrefsPanel::drawStreakItemsScrollbar(const HDC hdc) const
{
	if (hndStkScroll_ == nullptr) {
		return;
	}

	RECT clientRect = {};
	GetClientRect(hndStkScroll_, &clientRect);
	drawRoundedPanel(hdc, clientRect, kScrollTrackFill, brighten(kCardBorder, 6), 10);

	RECT innerRect = clientRect;
	InflateRect(&innerRect, -1, -1);
	drawRoundedPanel(hdc, innerRect, kInputFill, darken(kCardBorder, 6), 9);

	RECT thumbRect = getStreakItemsScrollThumbRect();
	const bool hasOverflow = getStreakItemsMaxTopLine() > 0;
	const COLORREF thumbFill = hasOverflow ? kScrollThumbFill : kScrollThumbDormant;
	const COLORREF thumbBorder = hasOverflow ? kScrollThumbBorder : brighten(kCardBorder, 10);

	drawRoundedPanel(hdc, thumbRect, thumbFill, thumbBorder, 8);

	RECT gripRect = thumbRect;
	InflateRect(&gripRect, -4, -10);
	if (gripRect.bottom - gripRect.top >= 10)
	{
		const HPEN gripPen = CreatePen(PS_SOLID, 1, hasOverflow ? RGB(11, 24, 36) : kTextMuted);
		const HGDIOBJ oldPen = SelectObject(hdc, gripPen);
		const int midY = (gripRect.top + gripRect.bottom) / 2;
		MoveToEx(hdc, gripRect.left, midY - 4, nullptr);
		LineTo(hdc, gripRect.right, midY - 4);
		MoveToEx(hdc, gripRect.left, midY, nullptr);
		LineTo(hdc, gripRect.right, midY);
		MoveToEx(hdc, gripRect.left, midY + 4, nullptr);
		LineTo(hdc, gripRect.right, midY + 4);
		SelectObject(hdc, oldPen);
		DeleteObject(gripPen);
	}
}

void PrefsPanel::syncStreakItemsScrollbar()
{
	if (hndStkScroll_ == nullptr) {
		return;
	}

	InvalidateRect(hndStkScroll_, nullptr, FALSE);
	UpdateWindow(hndStkScroll_);
}

void PrefsPanel::setStreakItemsTopLine(const int topLine)
{
	if (hndStkItems_ == nullptr) {
		return;
	}

	const int maxTopLine = getStreakItemsMaxTopLine();
	const int clampedTopLine = (std::max)(0, (std::min)(maxTopLine, topLine));
	const int firstVisibleLine = static_cast<int>(SendMessageW(hndStkItems_, EM_GETFIRSTVISIBLELINE, 0, 0));
	const int lineDelta = clampedTopLine - firstVisibleLine;
	if (lineDelta != 0) {
		SendMessageW(hndStkItems_, EM_LINESCROLL, 0, lineDelta);
	}

	syncStreakItemsScrollbar();
}

void PrefsPanel::beginStreakItemsScrollDrag(const int mouseY)
{
	if (hndStkScroll_ == nullptr) {
		return;
	}

	RECT clientRect = {};
	GetClientRect(hndStkScroll_, &clientRect);
	const RECT thumbRect = getStreakItemsScrollThumbRect();
	if (mouseY >= thumbRect.top && mouseY <= thumbRect.bottom)
	{
		stkScrollDrag_ = true;
		stkScrollOff_ = mouseY - thumbRect.top;
		SetCapture(hndStkScroll_);
		return;
	}

	const int trackPadding = 3;
	const int thumbHeight = thumbRect.bottom - thumbRect.top;
	const int trackTop = static_cast<int>(clientRect.top) + trackPadding;
	const int trackBottom = (std::max)(trackTop + 1, static_cast<int>(clientRect.bottom) - trackPadding);
	const int thumbTravel = (std::max)(1, (trackBottom - trackTop) - thumbHeight);
	const int maxTopLine = getStreakItemsMaxTopLine();
	const int targetTopLine = maxTopLine == 0
		? 0
		: MulDiv((std::max)(0, mouseY - trackTop - (thumbHeight / 2)), maxTopLine, thumbTravel);
	setStreakItemsTopLine(targetTopLine);
}

void PrefsPanel::updateStreakItemsScrollDrag(const int mouseY)
{
	if (!stkScrollDrag_ || hndStkScroll_ == nullptr) {
		return;
	}

	RECT clientRect = {};
	GetClientRect(hndStkScroll_, &clientRect);
	const RECT thumbRect = getStreakItemsScrollThumbRect();
	const int trackPadding = 3;
	const int trackTop = static_cast<int>(clientRect.top) + trackPadding;
	const int trackBottom = static_cast<int>(clientRect.bottom) - trackPadding;
	const int thumbHeight = thumbRect.bottom - thumbRect.top;
	const int thumbTravel = (std::max)(1, (trackBottom - trackTop) - thumbHeight);
	const int maxTopLine = getStreakItemsMaxTopLine();
	const int thumbTop = (std::max)(trackTop, (std::min)(trackBottom - thumbHeight, mouseY - stkScrollOff_));
	const int targetTopLine = maxTopLine == 0
		? 0
		: MulDiv(thumbTop - trackTop, maxTopLine, thumbTravel);
	setStreakItemsTopLine(targetTopLine);
}

void PrefsPanel::endStreakItemsScrollDrag()
{
	if (!stkScrollDrag_) {
		return;
	}

	stkScrollDrag_ = false;
	if (GetCapture() == hndStkScroll_) {
		ReleaseCapture();
	}
}

void PrefsPanel::updateStreakToggleControls() const
{
	const HWND survivorToggle = GetDlgItem(nativeWindow_, CMD_CHK_STREAK_SURV);
	const HWND killerToggle = GetDlgItem(nativeWindow_, CMD_CHK_STREAK_KLR);

	if (survivorToggle != nullptr)
	{
		Button_SetCheck(survivorToggle, editedConfig.streakData.activeOverlay == 1 ? BST_CHECKED : BST_UNCHECKED);
		RedrawWindow(survivorToggle, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
	}
	if (killerToggle != nullptr)
	{
		Button_SetCheck(killerToggle, editedConfig.streakData.activeOverlay == 2 ? BST_CHECKED : BST_UNCHECKED);
		RedrawWindow(killerToggle, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
	}
}

void PrefsPanel::updateOverlayToggleControls() const
{
	if (hndOtxKey_ != nullptr) {
		RedrawWindow(hndOtxKey_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
	}
	if (hndOtxTimed_ != nullptr) {
		RedrawWindow(hndOtxTimed_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
	}
}

void PrefsPanel::setBackdropMenuVisible(const bool isOpen)
{
	backdropOpen_ = isOpen && currentTab == SettingsPage::PageTimers;

	if (hndBackdropSurface_ != nullptr)
	{
		ShowWindow(hndBackdropSurface_, SW_HIDE);
	}

	const int optionCheckboxIds[] = {
		CMD_CHK_AUTO_BEGIN,
		CMD_CHK_SEETHROUGH,
		CMD_CHK_PASSTHRU,
		CMD_CHK_TIMER_IMAGE
	};
	for (const int checkboxId : optionCheckboxIds)
	{
		const HWND checkbox = GetDlgItem(nativeWindow_, checkboxId);
		if (checkbox != nullptr) {
			ShowWindow(checkbox, currentTab == SettingsPage::PageTimers && !backdropOpen_ ? SW_SHOW : SW_HIDE);
		}
	}

	for (HWND button : hndBackdropOpts_)
	{
		if (button == nullptr) {
			continue;
		}

		ShowWindow(button, backdropOpen_ ? SW_SHOW : SW_HIDE);
		if (backdropOpen_) {
			SetWindowPos(button, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			RedrawWindow(button, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
		}
	}

	if (hndBackdropBtn_ != nullptr) {
		RedrawWindow(hndBackdropBtn_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
	}

	RECT optionsRect = { 14, 456, WIN_PREFS_W - 14, 630 };
	InvalidateRect(nativeWindow_, &optionsRect, FALSE);
}

bool PrefsPanel::pointHitsBackdropMenu(const POINT point) const
{
	if (hndBackdropBtn_ != nullptr)
	{
		RECT gearRect = {};
		GetWindowRect(hndBackdropBtn_, &gearRect);
		MapWindowPoints(HWND_DESKTOP, nativeWindow_, reinterpret_cast<LPPOINT>(&gearRect), 2);
		if (PtInRect(&gearRect, point)) {
			return true;
		}
	}

	if (hndBackdropSurface_ != nullptr && IsWindowVisible(hndBackdropSurface_))
	{
		RECT panelRect = {};
		GetWindowRect(hndBackdropSurface_, &panelRect);
		MapWindowPoints(HWND_DESKTOP, nativeWindow_, reinterpret_cast<LPPOINT>(&panelRect), 2);
		if (PtInRect(&panelRect, point)) {
			return true;
		}
	}

	for (HWND button : hndBackdropOpts_)
	{
		if (button == nullptr || !IsWindowVisible(button)) {
			continue;
		}

		RECT buttonRect = {};
		GetWindowRect(button, &buttonRect);
		MapWindowPoints(HWND_DESKTOP, nativeWindow_, reinterpret_cast<LPPOINT>(&buttonRect), 2);
		if (PtInRect(&buttonRect, point)) {
			return true;
		}
	}

	return false;
}

void PrefsPanel::applyBrandingFont(const HWND hControl)
{
	const HFONT hFont = CreateFont(
		13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
	);

	SendMessage(hControl, WM_SETFONT, (WPARAM)hFont, TRUE);
}

bool PrefsPanel::applyDurationInputs()
{
	wchar_t firstDurationText[32] = {};
	wchar_t secondDurationText[32] = {};
	GetWindowText(hndTmr1Dur_, firstDurationText, 31);
	GetWindowText(hndTmr2Dur_, secondDurationText, 31);

	int firstDurationMs = 0;
	int secondDurationMs = 0;
	if (!parseDurationTextLocal(firstDurationText, firstDurationMs) || !parseDurationTextLocal(secondDurationText, secondDurationMs))
	{
		MessageBox(nativeWindow_, L"Use ss, mm:ss or hh:mm:ss for both timers.", L"Invalid time", MB_OK | MB_ICONWARNING);
		SetWindowText(hndTmr1Dur_, formatDurationTextLocal(editedConfig.timerOneDuration).c_str());
		SetWindowText(hndTmr2Dur_, formatDurationTextLocal(editedConfig.timerTwoDuration).c_str());
		return false;
	}

	editedConfig.timerOneDuration = firstDurationMs;
	editedConfig.timerTwoDuration = secondDurationMs;
	return true;
}

void PrefsPanel::onWidgetNotify(const WPARAM firstParam, const LPARAM secondParam)
{
	const HWND controlHandle = reinterpret_cast<HWND>(secondParam);
	const int controlId = LOWORD(firstParam);
	const int notificationCode = HIWORD(firstParam);

	switch (controlId) {
	case CMD_ACCEPT:
		if (controlHandle == nullptr) {
			return;
		}

		if (!applyDurationInputs()) {
			return;
		}
		if (!applyOverlayInputs(true)) {
			return;
		}
		applyWheelInputs(false);
		syncStreakPresetFromInputs();
		editedConfig.loadoutData = appSettings_.loadoutData;

		applySave(editedConfig);
		cfgApplied_ = true;
		if (chatOverlay != nullptr) {
			chatOverlay->applyConfig(appSettings_.notifyProfile);
		}
		SendMessage(GetWindow(nativeWindow_, GW_OWNER), WM_REFRESH_TINTS, 0, 0);
		DestroyWindow(nativeWindow_);
		break;
	case CMD_DISMISS:
		if (controlHandle == nullptr) {
			return;
		}

		if (chatOverlay != nullptr) {
			chatOverlay->applyConfig(appSettings_.notifyProfile);
		}
		DestroyWindow(nativeWindow_);
		break;
	case CMD_TIMER_ONE:
	case CMD_TIMER_TWO:
	case CMD_PAD_BIND_BEGIN:
	case CMD_PAD_BIND_TMR1:
	case CMD_PAD_BIND_TMR2:
	case CMD_PAD_BIND_BEGIN_NR:
		if (controlHandle == nullptr) {
			return;
		}

		SetFocus(nativeWindow_);
		activeHotkeyControl_ = controlHandle;
		SetWindowText(activeHotkeyControl_, L"...");
		RedrawWindow(controlHandle, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
		break;
	case CMD_PAGE_TIMERS:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED) {
	setActiveTab(SettingsPage::PageTimers);
		}
		break;
	case CMD_PAGE_WHEEL:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED) {
			setActiveTab(SettingsPage::PageWheel);
		}
		break;
	case CMD_PAGE_BUILDS:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED) {
			setActiveTab(SettingsPage::PageBuilds);
		}
		break;
	case CMD_PAGE_EXTRA:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED) {
			setActiveTab(SettingsPage::PageExtra);
		}
		break;
	case CMD_PAGE_OVERLAYS:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED) {
			setActiveTab(SettingsPage::PageOverlays);
		}
		break;
	case CMD_OVERLAY_KEY:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED)
		{
			SetFocus(nativeWindow_);
			activeHotkeyControl_ = controlHandle;
			SetWindowText(activeHotkeyControl_, L"...");
			RedrawWindow(controlHandle, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
		}
		break;
	case CMD_CHK_OVERLAY_TIMED:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED)
		{
			const UINT nextState = editedConfig.notifyProfile.toxicChatTimed ? BST_UNCHECKED : BST_CHECKED;
			Button_SetCheck(controlHandle, nextState);
			editedConfig.notifyProfile.toxicChatTimed = nextState == BST_CHECKED;
			RedrawWindow(controlHandle, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
		}
		break;
	case CMD_OVERLAY_BROWSE:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED) {
			browseForOverlayImage();
		}
		break;
	case CMD_CHK_STREAK_SURV:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED)
		{
			editedConfig.streakData.activeOverlay = editedConfig.streakData.activeOverlay == 1 ? 0 : 1;
			selectStreakPreset(0);
			updateStreakToggleControls();
		}
		break;
	case CMD_CHK_STREAK_KLR:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED)
		{
			editedConfig.streakData.activeOverlay = editedConfig.streakData.activeOverlay == 2 ? 0 : 2;
			selectStreakPreset(1);
			updateStreakToggleControls();
		}
		break;
	case CMD_STREAK_PRESET:
		if (controlHandle != nullptr && notificationCode == CBN_SELCHANGE)
		{
			const int selectedPreset = static_cast<int>(SendMessageW(controlHandle, CB_GETCURSEL, 0, 0));
			if (selectedPreset != CB_ERR) {
				selectStreakPreset(selectedPreset);
			}
		}
		break;
	case CMD_STREAK_ITEMS:
		if (notificationCode == EN_CHANGE || notificationCode == EN_UPDATE) {
			syncStreakItemsScrollbar();
		}
		break;
	case CMD_CHK_AUTO_BEGIN:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED)
		{
			const UINT nextState = editedConfig.flagAutoStart ? BST_UNCHECKED : BST_CHECKED;
			Button_SetCheck(controlHandle, nextState);
			editedConfig.flagAutoStart = nextState == BST_CHECKED;
			RedrawWindow(controlHandle, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
		}
		break;
	case CMD_CHK_COUNTUP:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED)
		{
			const UINT nextState = editedConfig.flagCountUp ? BST_UNCHECKED : BST_CHECKED;
			Button_SetCheck(controlHandle, nextState);
			editedConfig.flagCountUp = nextState == BST_CHECKED;
			RedrawWindow(controlHandle, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
		}
		break;
	case CMD_CHK_TIMER_IMAGE:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED)
		{
			const UINT nextState = editedConfig.flagTimerImg ? BST_UNCHECKED : BST_CHECKED;
			Button_SetCheck(controlHandle, nextState);
			editedConfig.flagTimerImg = nextState == BST_CHECKED;
			RedrawWindow(controlHandle, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
		}
		break;
	case CMD_CHK_SEETHROUGH:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED)
		{
			const UINT nextState = editedConfig.flagSeeThrough ? BST_UNCHECKED : BST_CHECKED;
			Button_SetCheck(controlHandle, nextState);
			editedConfig.flagSeeThrough = nextState == BST_CHECKED;
			RedrawWindow(controlHandle, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
		}
		break;
	case CMD_CHK_PASSTHRU:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED)
		{
			const UINT nextState = editedConfig.flagPassThru ? BST_UNCHECKED : BST_CHECKED;
			Button_SetCheck(controlHandle, nextState);
			editedConfig.flagPassThru = nextState == BST_CHECKED;
			RedrawWindow(controlHandle, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
		}
		break;
	case CMD_BG_STYLE_GEAR:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED) {
			setBackdropMenuVisible(!backdropOpen_);
		}
		break;
	case CMD_BG_STYLE_CUR:
	case CMD_BG_STYLE_BLK:
	case CMD_BG_STYLE_WHT:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED)
		{
			editedConfig.flagBgStyle = controlId - CMD_BG_STYLE_CUR;
			setBackdropMenuVisible(false);
			RedrawWindow(nativeWindow_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
		}
		break;
	case CMD_WHEEL_BROWSE:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED) {
			browseForWheelImage();
		}
		break;
	case CMD_WHEEL_PRESET:
		if (controlHandle != nullptr && notificationCode == CBN_SELCHANGE)
		{
			const int selectedPreset = static_cast<int>(SendMessageW(controlHandle, CB_GETCURSEL, 0, 0));
			if (selectedPreset != CB_ERR) {
				selectWheelPreset(selectedPreset);
			}
		}
		break;
	case CMD_WHEEL_RENAME:
		if (notificationCode == EN_KILLFOCUS) {
			commitWheelPresetRename(true);
		}
		break;
	case CMD_WHEEL_ITEMS:
		if (notificationCode == EN_CHANGE || notificationCode == EN_UPDATE) {
			syncWheelItemsScrollbar();
		}
		break;
	case CMD_WHEEL_SHOW:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED)
		{
			if (!applyWheelInputs(true)) {
				return;
			}

			if (pSpinnerWheel != nullptr) {
				pSpinnerWheel->showUpdate(editedConfig.wheelData, nativeWindow_);
			}
		}
		break;
	case CMD_BUILD_SURV:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED && pSideSurvivorLoadoutEditor != nullptr) {
			pSideSurvivorLoadoutEditor->showForSide(LoadoutSide::SideSurvivor, nativeWindow_);
		}
		break;
	case CMD_BUILD_KLR:
		if (controlHandle != nullptr && notificationCode == BN_CLICKED && pSideKillerLoadoutEditor != nullptr) {
			pSideKillerLoadoutEditor->showForSide(LoadoutSide::SideKiller, nativeWindow_);
		}
		break;
	case CMD_CLR_TIMER:
	case CMD_CLR_SEL_TIMER:
	case CMD_CLR_URGENT:
	case CMD_CLR_BACKDROP:
		if (controlHandle == nullptr) {
			return;
		}

		if (colorPicker != nullptr)
		{
			if (colorPicker->id() == nullptr)
			{
				colorPicker->originCtrlId = controlId;
				colorPicker->parentCfg = &editedConfig;

			if (!colorPicker->spawn(
					850, 300,
					DLG_TINT_W, DLG_TINT_H,
					0,
					WS_POPUP,
					nativeWindow_,
					L"Color Picker"))
				{
					return;
				}
				ShowWindow(colorPicker->id(), SW_SHOW);
			}
			else
			{
				SetForegroundWindow(colorPicker->id());
			}
		}
		break;
	default:
		break;
	}
}

void PrefsPanel::onColorMsg(const LPARAM secondParam) const
{
	drawOwnerDrawControl(reinterpret_cast<const DRAWITEMSTRUCT*>(secondParam));
}

void PrefsPanel::drawThemeButton(const DRAWITEMSTRUCT* drawItem) const
{
	wchar_t text[64] = {};
	GetWindowText(drawItem->hwndItem, text, 63);

	const bool isPrimary = drawItem->CtlID == CMD_ACCEPT;
	const bool isCancel = drawItem->CtlID == CMD_DISMISS;
	const bool isTabButton = drawItem->CtlID == CMD_PAGE_TIMERS || drawItem->CtlID == CMD_PAGE_WHEEL ||
		drawItem->CtlID == CMD_PAGE_BUILDS || drawItem->CtlID == CMD_PAGE_EXTRA ||
		drawItem->CtlID == CMD_PAGE_OVERLAYS;
	const bool isListening = activeHotkeyControl_ == drawItem->hwndItem;
	const bool isPressed = (drawItem->itemState & ODS_SELECTED) != 0;
	const bool isActiveTab =
		(drawItem->CtlID == CMD_PAGE_TIMERS && currentTab == SettingsPage::PageTimers) ||
		(drawItem->CtlID == CMD_PAGE_WHEEL && currentTab == SettingsPage::PageWheel) ||
		(drawItem->CtlID == CMD_PAGE_BUILDS && currentTab == SettingsPage::PageBuilds) ||
		(drawItem->CtlID == CMD_PAGE_EXTRA && currentTab == SettingsPage::PageExtra) ||
		(drawItem->CtlID == CMD_PAGE_OVERLAYS && currentTab == SettingsPage::PageOverlays);

	COLORREF fillColor = isPrimary ? kOkFill : kButtonFill;
	COLORREF borderColor = isPrimary ? brighten(kOkFill, 18) : kCardBorder;
	COLORREF textColor = isPrimary ? RGB(9, 16, 24) : kTextPrimary;

	if (isTabButton)
	{
		fillColor = isActiveTab ? RGB(38, 170, 245) : RGB(10, 16, 25);
		borderColor = isActiveTab ? RGB(114, 216, 255) : RGB(28, 43, 64);
		textColor = isActiveTab ? RGB(5, 14, 22) : brighten(kTextMuted, 28);
	}

	if (isCancel) {
		fillColor = kCancelFill;
		borderColor = brighten(kCardBorder, 6);
	}

	if (isListening) {
		fillColor = darken(kAccent, 18);
		borderColor = kAccentSoft;
		textColor = kAccentSoft;
	}

	if (isPressed) {
		fillColor = darken(fillColor, 16);
	}

	if (isTabButton)
	{
		const HBRUSH bgBrush = CreateSolidBrush(RGB(9, 14, 22));
		FillRect(drawItem->hDC, &drawItem->rcItem, bgBrush);
		DeleteObject(bgBrush);

		RECT tabRect = drawItem->rcItem;
		InflateRect(&tabRect, -2, -2);
		drawRoundedPanel(drawItem->hDC, tabRect, fillColor, borderColor, 12);
	}
	else
	{
		drawRoundedPanel(drawItem->hDC, drawItem->rcItem, fillColor, borderColor, 12);
	}

	RECT textRect = drawItem->rcItem;
	SetBkMode(drawItem->hDC, TRANSPARENT);
	SetTextColor(drawItem->hDC, textColor);
	DrawTextW(drawItem->hDC, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void PrefsPanel::drawThemeCheckbox(const DRAWITEMSTRUCT* drawItem) const
{
	const bool isChecked = isCheckboxChecked(drawItem->CtlID);
	const bool isPressed = (drawItem->itemState & ODS_SELECTED) != 0;

	RECT boxRect = drawItem->rcItem;
	InflateRect(&boxRect, 1, 1);

	COLORREF fillColor = RGB(250, 252, 255);
	COLORREF borderColor = isChecked ? brighten(kAccentSoft, 8) : RGB(214, 224, 236);
	if (isPressed) {
		fillColor = darken(fillColor, 14);
	}

	drawRoundedPanel(drawItem->hDC, boxRect, fillColor, borderColor, 7);

	RECT innerRect = boxRect;
	InflateRect(&innerRect, -3, -3);
	drawRoundedPanel(drawItem->hDC, innerRect, RGB(255, 255, 255), RGB(255, 255, 255), 5);

	if (isChecked)
	{
		const HPEN pen = CreatePen(PS_SOLID, 3, kAccent);
		const HGDIOBJ oldPen = SelectObject(drawItem->hDC, pen);
		MoveToEx(drawItem->hDC, innerRect.left + 3, innerRect.top + 8, nullptr);
		LineTo(drawItem->hDC, innerRect.left + 7, innerRect.bottom - 4);
		LineTo(drawItem->hDC, innerRect.right - 3, innerRect.top + 4);
		SelectObject(drawItem->hDC, oldPen);
		DeleteObject(pen);
	}
}

void PrefsPanel::drawBackdropLauncher(const DRAWITEMSTRUCT* drawItem) const
{
	if (drawItem == nullptr) {
		return;
	}

	RECT rect = drawItem->rcItem;
	const bool isPressed = (drawItem->itemState & ODS_SELECTED) != 0;
	const COLORREF fillColor = isPressed ? RGB(12, 42, 64) : RGB(10, 20, 34);
	const COLORREF borderColor = backdropOpen_ ? kAccentSoft : kAccent;

	const HBRUSH bgBrush = CreateSolidBrush(kCardFill);
	FillRect(drawItem->hDC, &rect, bgBrush);
	DeleteObject(bgBrush);

	Gdiplus::Graphics graphics(drawItem->hDC);
	graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

	Gdiplus::RectF circleRect(
		static_cast<Gdiplus::REAL>(rect.left + 2),
		static_cast<Gdiplus::REAL>(rect.top + 2),
		static_cast<Gdiplus::REAL>(rect.right - rect.left - 4),
		static_cast<Gdiplus::REAL>(rect.bottom - rect.top - 4));
	Gdiplus::SolidBrush fillBrush(Gdiplus::Color(255, GetRValue(fillColor), GetGValue(fillColor), GetBValue(fillColor)));
	Gdiplus::Pen borderPen(Gdiplus::Color(255, GetRValue(borderColor), GetGValue(borderColor), GetBValue(borderColor)), 1.8f);
	graphics.FillEllipse(&fillBrush, circleRect);
	graphics.DrawEllipse(&borderPen, circleRect);

	Gdiplus::Pen linePen(Gdiplus::Color(255, GetRValue(kAccentSoft), GetGValue(kAccentSoft), GetBValue(kAccentSoft)), 1.8f);
	linePen.SetStartCap(Gdiplus::LineCapRound);
	linePen.SetEndCap(Gdiplus::LineCapRound);
	const Gdiplus::REAL centerX = static_cast<Gdiplus::REAL>((rect.left + rect.right) / 2);
	const Gdiplus::REAL centerY = static_cast<Gdiplus::REAL>((rect.top + rect.bottom) / 2);
	for (int index = -1; index <= 1; ++index)
	{
		const Gdiplus::REAL y = centerY + static_cast<Gdiplus::REAL>(index * 4);
		graphics.DrawLine(&linePen, centerX - 5.5f, y, centerX + 5.5f, y);
	}
}

void PrefsPanel::drawBackdropPalette(const DRAWITEMSTRUCT* drawItem) const
{
	if (drawItem == nullptr) {
		return;
	}

	const HBRUSH bgBrush = CreateSolidBrush(kCardFill);
	FillRect(drawItem->hDC, &drawItem->rcItem, bgBrush);
	DeleteObject(bgBrush);

	RECT panelRect = drawItem->rcItem;
	drawRoundedPanel(drawItem->hDC, panelRect, RGB(5, 10, 16), RGB(42, 75, 105), 14);
}

void PrefsPanel::drawBackdropSwatch(const DRAWITEMSTRUCT* drawItem) const
{
	if (drawItem == nullptr) {
		return;
	}

	const int styleIndex = drawItem->CtlID - CMD_BG_STYLE_CUR;
	const bool isSelected = editedConfig.flagBgStyle == styleIndex;
	const bool isPressed = (drawItem->itemState & ODS_SELECTED) != 0;
	const COLORREF fillColor = isSelected ? RGB(18, 46, 67) : RGB(7, 12, 19);
	const COLORREF borderColor = isSelected ? kAccentSoft : RGB(38, 56, 78);
	drawRoundedPanel(drawItem->hDC, drawItem->rcItem, isPressed ? darken(fillColor, 8) : fillColor, borderColor, 10);

	RECT swatchRect = drawItem->rcItem;
	swatchRect.left += 10;
	swatchRect.right = swatchRect.left + 18;
	swatchRect.top += 7;
	swatchRect.bottom -= 7;
	const COLORREF swatchColor = styleIndex == 0
		? RGB(37, 63, 79)
		: styleIndex == 1
			? RGB(0, 0, 0)
			: RGB(255, 255, 255);
	drawRoundedPanel(drawItem->hDC, swatchRect, swatchColor, styleIndex == 2 ? RGB(190, 203, 218) : kAccent, 6);

	wchar_t text[64] = {};
	GetWindowText(drawItem->hwndItem, text, 63);
	RECT textRect = drawItem->rcItem;
	textRect.left += 36;
	textRect.right -= 24;
	SetBkMode(drawItem->hDC, TRANSPARENT);
	SetTextColor(drawItem->hDC, kTextPrimary);
	DrawTextW(drawItem->hDC, text, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);

	if (isSelected)
	{
		const HPEN checkPen = CreatePen(PS_SOLID, 2, kAccentSoft);
		const HGDIOBJ oldPen = SelectObject(drawItem->hDC, checkPen);
		const int right = drawItem->rcItem.right - 10;
		const int midY = (drawItem->rcItem.top + drawItem->rcItem.bottom) / 2;
		MoveToEx(drawItem->hDC, right - 10, midY, nullptr);
		LineTo(drawItem->hDC, right - 6, midY + 4);
		LineTo(drawItem->hDC, right, midY - 5);
		SelectObject(drawItem->hDC, oldPen);
		DeleteObject(checkPen);
	}
}

void PrefsPanel::drawThemePresetComboItem(const DRAWITEMSTRUCT* drawItem) const
{
	if (drawItem == nullptr || drawItem->hwndItem == nullptr || drawItem->itemID == static_cast<UINT>(-1)) {
		return;
	}

	wchar_t text[128] = {};
	SendMessageW(drawItem->hwndItem, CB_GETLBTEXT, drawItem->itemID, reinterpret_cast<LPARAM>(text));

	RECT itemRect = drawItem->rcItem;
	const bool isSelected = (drawItem->itemState & ODS_SELECTED) != 0;
	const bool isInList = (drawItem->itemState & ODS_COMBOBOXEDIT) == 0;

	COLORREF fillColor = isSelected ? darken(kAccent, 20) : kInputFill;
	COLORREF borderColor = isSelected ? kAccentSoft : darken(kCardBorder, 8);
	COLORREF textColor = isSelected ? RGB(244, 250, 255) : kTextPrimary;

	if (!isInList)
	{
		fillColor = kInputFill;
		borderColor = kInputFill;
	}

	drawRoundedPanel(drawItem->hDC, itemRect, fillColor, borderColor, 8);

	RECT textRect = itemRect;
	textRect.left += 10;
	textRect.right -= 8;
	SetBkMode(drawItem->hDC, TRANSPARENT);
	SetTextColor(drawItem->hDC, textColor);
	DrawTextW(drawItem->hDC, text, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
}

void PrefsPanel::paintThemePresetCombo(const HWND comboHwnd, HDC hdc) const
{
	if (comboHwnd == nullptr) {
		return;
	}

	COMBOBOXINFO comboInfo = {};
	comboInfo.cbSize = sizeof(COMBOBOXINFO);
	if (!GetComboBoxInfo(comboHwnd, &comboInfo)) {
		return;
	}

	RECT clientRect = {};
	GetClientRect(comboHwnd, &clientRect);
	MapWindowPoints(HWND_DESKTOP, comboHwnd, reinterpret_cast<LPPOINT>(&comboInfo.rcButton), 2);

	const bool ownsDc = hdc == nullptr;
	if (ownsDc) {
		hdc = GetDC(comboHwnd);
	}
	if (hdc == nullptr) {
		return;
	}

	const HBRUSH bgBrush = CreateSolidBrush(kWheelCardFill);
	FillRect(hdc, &clientRect, bgBrush);
	DeleteObject(bgBrush);

	RECT outerRect = clientRect;
	drawRoundedPanel(hdc, outerRect, kInputFill, brighten(kCardBorder, 12), 8);

	RECT contentRect = outerRect;
	InflateRect(&contentRect, -1, -1);

	RECT buttonRect = contentRect;
	buttonRect.left = (std::max)(contentRect.left + 24, contentRect.right - 24);
	buttonRect.top += 2;
	buttonRect.bottom -= 2;
	buttonRect.right -= 2;
	drawRoundedPanel(hdc, buttonRect, kButtonFill, brighten(kCardBorder, 12), 7);

	const HPEN dividerPen = CreatePen(PS_SOLID, 1, darken(kCardBorder, 10));
	const HGDIOBJ oldDividerPen = SelectObject(hdc, dividerPen);
	MoveToEx(hdc, buttonRect.left, contentRect.top + 4, nullptr);
	LineTo(hdc, buttonRect.left, contentRect.bottom - 4);
	SelectObject(hdc, oldDividerPen);
	DeleteObject(dividerPen);

	wchar_t selectedText[128] = {};
	const int selectedIndex = static_cast<int>(SendMessageW(comboHwnd, CB_GETCURSEL, 0, 0));
	if (selectedIndex != CB_ERR) {
		SendMessageW(comboHwnd, CB_GETLBTEXT, selectedIndex, reinterpret_cast<LPARAM>(selectedText));
	}

	RECT textRect = contentRect;
	textRect.left += 10;
	textRect.right = buttonRect.left - 6;
	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, kTextPrimary);
	DrawTextW(hdc, selectedText, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);

	const int arrowCenterX = buttonRect.left + ((buttonRect.right - buttonRect.left) / 2);
	const int arrowCenterY = buttonRect.top + ((buttonRect.bottom - buttonRect.top) / 2);
	const HPEN arrowPen = CreatePen(PS_SOLID, 2, kAccentSoft);
	const HGDIOBJ oldArrowPen = SelectObject(hdc, arrowPen);
	MoveToEx(hdc, arrowCenterX - 4, arrowCenterY - 2, nullptr);
	LineTo(hdc, arrowCenterX, arrowCenterY + 2);
	LineTo(hdc, arrowCenterX + 4, arrowCenterY - 2);
	SelectObject(hdc, oldArrowPen);
	DeleteObject(arrowPen);

	if (ownsDc) {
		ReleaseDC(comboHwnd, hdc);
	}
}

bool PrefsPanel::isCheckboxChecked(const int controlId) const
{
	switch (controlId)
	{
	case CMD_CHK_AUTO_BEGIN:
		return editedConfig.flagAutoStart;
	case CMD_CHK_COUNTUP:
		return editedConfig.flagCountUp;
	case CMD_CHK_TIMER_IMAGE:
		return editedConfig.flagTimerImg;
	case CMD_CHK_SEETHROUGH:
		return editedConfig.flagSeeThrough;
	case CMD_CHK_PASSTHRU:
		return editedConfig.flagPassThru;
	case CMD_CHK_STREAK_SURV:
		return editedConfig.streakData.activeOverlay == 1;
	case CMD_CHK_STREAK_KLR:
		return editedConfig.streakData.activeOverlay == 2;
	case CMD_CHK_OVERLAY_TIMED:
		return editedConfig.notifyProfile.toxicChatTimed;
	default:
		return false;
	}
}

void PrefsPanel::drawThemeColorButton(const DRAWITEMSTRUCT* drawItem) const
{
	COLORREF fillColor = kAccent;

	switch (drawItem->CtlID)
	{
	case CMD_CLR_TIMER:
		fillColor = colorFromBrush(paletteBrushes_[editedConfig.tintSelection.timerOneColor]);
		break;
	case CMD_CLR_SEL_TIMER:
		fillColor = colorFromBrush(paletteBrushes_[editedConfig.tintSelection.timerTwoColor]);
		break;
	case CMD_CLR_URGENT:
		fillColor = colorFromBrush(paletteBrushes_[editedConfig.tintSelection.urgentColor]);
		break;
	case CMD_CLR_BACKDROP:
		fillColor = colorFromBrush(paletteBrushes_[editedConfig.tintSelection.backdropColor]);
		break;
	default:
		break;
	}

	drawRoundedPanel(drawItem->hDC, drawItem->rcItem, kButtonFill, brighten(kCardBorder, 12), 12);

	RECT innerRect = drawItem->rcItem;
	InflateRect(&innerRect, -6, -4);
	drawRoundedPanel(drawItem->hDC, innerRect, fillColor, brighten(fillColor, 8), 10);
}

void PrefsPanel::drawOwnerDrawControl(const DRAWITEMSTRUCT* drawItem) const
{
	if (drawItem == nullptr) {
		return;
	}

	if (drawItem->CtlID == CMD_WHEEL_PRESET || drawItem->CtlID == CMD_STREAK_PRESET) {
		drawThemePresetComboItem(drawItem);
		return;
	}

	if (drawItem->CtlID == CMD_BG_STYLE_GEAR) {
		drawBackdropLauncher(drawItem);
		return;
	}

	if (drawItem->CtlID == CMD_BG_STYLE_PNL) {
		drawBackdropPalette(drawItem);
		return;
	}

	if (isBackdropChoiceId(drawItem->CtlID)) {
		drawBackdropSwatch(drawItem);
		return;
	}

	if (isColorControlId(drawItem->CtlID)) {
		drawThemeColorButton(drawItem);
		return;
	}

	if (isCheckboxControlId(drawItem->CtlID)) {
		drawThemeCheckbox(drawItem);
		return;
	}

	drawThemeButton(drawItem);
}

bool PrefsPanel::isCheckboxControlId(const int controlId)
{
	return controlId == CMD_CHK_AUTO_BEGIN ||
		controlId == CMD_CHK_COUNTUP ||
		controlId == CMD_CHK_TIMER_IMAGE ||
		controlId == CMD_CHK_SEETHROUGH ||
		controlId == CMD_CHK_PASSTHRU ||
		controlId == CMD_CHK_STREAK_SURV ||
		controlId == CMD_CHK_STREAK_KLR ||
		controlId == CMD_CHK_OVERLAY_TIMED;
}

bool PrefsPanel::isColorControlId(const int controlId)
{
	return controlId == CMD_CLR_TIMER ||
		controlId == CMD_CLR_SEL_TIMER ||
		controlId == CMD_CLR_URGENT ||
		controlId == CMD_CLR_BACKDROP;
}

bool PrefsPanel::isBackdropChoiceId(const int controlId)
{
	return controlId == CMD_BG_STYLE_CUR ||
		controlId == CMD_BG_STYLE_BLK ||
		controlId == CMD_BG_STYLE_WHT;
}

bool PrefsPanel::isTitleText(const wchar_t* text)
{
	return textEquals(text, L"Timers") ||
		textEquals(text, L"Hotkeys") ||
		textEquals(text, L"Options") ||
		textEquals(text, L"Colors") ||
		textEquals(text, L"Roulette") ||
		textEquals(text, L"Builds") ||
		textEquals(text, L"Extra") ||
		textEquals(text, L"Overlays") ||
		textEquals(text, L"Streaks") ||
		textEquals(text, L"Toxic Chat") ||
		textEquals(text, L"Background Image") ||
		textEquals(text, L"SideSurvivor") ||
		textEquals(text, L"SideKiller") ||
		textEquals(text, L"Data") ||
		textEquals(text, L"Preset") ||
		textEquals(text, L"Sections") ||
		textEquals(text, L"Presets") ||
		textEquals(text, L"Center Image");
}

bool PrefsPanel::isMutedText(const wchar_t* text)
{
	return textEquals(text, L"(c) Truueh 2025") ||
		textEquals(text, L"\u00A9 VyxenApps 2026") ||
		textStartsWith(text, L"Use ss") ||
		textEquals(text, L"image") ||
		textEquals(text, L"imagen") ||
		textEquals(text, L"Keys / Mouse") ||
		textEquals(text, L"Controller") ||
		textStartsWith(text, L"One name per line") ||
		textStartsWith(text, L"Double click") ||
		textStartsWith(text, L"Double click") ||
		textStartsWith(text, L"One name per line") ||
		textStartsWith(text, L"Line 1") ||
		textStartsWith(text, L"Cover the") ||
		textEquals(text, L"Chat Overlay") ||
		textEquals(text, L"Wins") ||
		textEquals(text, L"Loss") ||
		textStartsWith(text, L"Secondary tab") ||
		textStartsWith(text, L"Use PNG");
}

void PrefsPanel::stashCtrlHotkey(const UINT key)
{
	const int controlId = GetDlgCtrlID(activeHotkeyControl_);

	if (controlId == CMD_OVERLAY_KEY) {
		return;
	}

	if (controlId == CMD_TIMER_ONE || controlId == CMD_TIMER_TWO) {
		return;
	}

	switch (controlId)
	{
	case CMD_PAD_BIND_BEGIN:
		editedConfig.padStartKey = key;
		break;
	case CMD_PAD_BIND_TMR1:
		editedConfig.padTimerOneKey = key;
		break;
	case CMD_PAD_BIND_TMR2:
		editedConfig.padTimerTwoKey = key;
		break;
	case CMD_PAD_BIND_BEGIN_NR:
		editedConfig.padStartNrKey = key;
		break;
	default:
		break;
	}

	SetWindowText(activeHotkeyControl_, lookupPad(key));
	RedrawWindow(activeHotkeyControl_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
	activeHotkeyControl_ = nullptr;
}

void PrefsPanel::stashKeyHotkey(const UINT key)
{
	const int controlId = GetDlgCtrlID(activeHotkeyControl_);

	if (controlId == CMD_PAD_BIND_BEGIN || controlId == CMD_PAD_BIND_TMR1 || controlId == CMD_PAD_BIND_TMR2 || controlId == CMD_PAD_BIND_BEGIN_NR)
	{
		if (key == VK_ESCAPE) {
			stashCtrlHotkey(PadBtn::PadMenu);
		}
		else if (key == VK_LBUTTON) {
			stashCtrlHotkey(PadBtn::PadRTrigger);
		}
		return;
	}

	switch (controlId)
	{
	case CMD_TIMER_ONE:
		editedConfig.timerOneKey = key;
		break;
	case CMD_TIMER_TWO:
		editedConfig.timerTwoKey = key;
		break;
	case CMD_OVERLAY_KEY:
		editedConfig.notifyProfile.toxicChatKey = key;
		break;
	default:
		break;
	}

	SetWindowText(activeHotkeyControl_, lookupKb(key));
	RedrawWindow(activeHotkeyControl_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
	activeHotkeyControl_ = nullptr;
}

void PrefsPanel::restoreHotkeyLabel(const HWND hCtrl)
{
	const int controlId = GetDlgCtrlID(hCtrl);

	switch (controlId) {
	case CMD_TIMER_ONE:
		SetWindowText(hCtrl, lookupKb(editedConfig.timerOneKey));
		break;
	case CMD_TIMER_TWO:
		SetWindowText(hCtrl, lookupKb(editedConfig.timerTwoKey));
		break;
	case CMD_PAD_BIND_BEGIN:
		SetWindowText(hCtrl, lookupPad(editedConfig.padStartKey));
		break;
	case CMD_PAD_BIND_TMR1:
		SetWindowText(hCtrl, lookupPad(editedConfig.padTimerOneKey));
		break;
	case CMD_PAD_BIND_TMR2:
		SetWindowText(hCtrl, lookupPad(editedConfig.padTimerTwoKey));
		break;
	case CMD_PAD_BIND_BEGIN_NR:
		SetWindowText(hCtrl, lookupPad(editedConfig.padStartNrKey));
		break;
	case CMD_OVERLAY_KEY:
		SetWindowText(hCtrl, lookupKb(editedConfig.notifyProfile.toxicChatKey));
		break;
	default:
		break;
	}
}

LRESULT PrefsPanel::processMessage(UINT windowMessage, WPARAM wideParameter, LPARAM longParameter)
{
	try
	{
		switch (windowMessage)
		{
		case WM_CREATE:
			layoutPanel();
			return 0;
		case WM_DESTROY:
			endWheelItemsScrollDrag();
			endStreakItemsScrollDrag();
			if (!cfgApplied_ && chatOverlay != nullptr) {
				chatOverlay->applyConfig(appSettings_.notifyProfile);
			}
			discardThemeResources();
			nativeWindow_ = nullptr;
			return 0;
		case WM_ERASEBKGND:
			return 1;
		case WM_COMMAND:
			onWidgetNotify(wideParameter, longParameter);
			return 0;
		case WM_MEASUREITEM:
		{
			MEASUREITEMSTRUCT* measureItem = reinterpret_cast<MEASUREITEMSTRUCT*>(longParameter);
			if (measureItem != nullptr && (measureItem->CtlID == CMD_WHEEL_PRESET || measureItem->CtlID == CMD_STREAK_PRESET))
			{
				measureItem->itemHeight = 24;
				return TRUE;
			}
			break;
		}
		case WM_DRAWITEM:
			onColorMsg(longParameter);
			return TRUE;
		case WM_CTLCOLORSTATIC:
		{
			const HDC hdc = reinterpret_cast<HDC>(wideParameter);
			const HWND control = reinterpret_cast<HWND>(longParameter);
			wchar_t text[96] = {};
			GetWindowText(control, text, 95);

			SetBkMode(hdc, TRANSPARENT);
			if (isTitleText(text)) {
				SetTextColor(hdc, kAccentSoft);
			}
			else if (isMutedText(text)) {
				SetTextColor(hdc, kTextMuted);
			}
			else {
				SetTextColor(hdc, kTextPrimary);
			}

			return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
		}
		case WM_CTLCOLOREDIT:
		{
			const HDC hdc = reinterpret_cast<HDC>(wideParameter);
			const HWND control = reinterpret_cast<HWND>(longParameter);
			if (control == hndWhlRename_)
			{
				SetTextColor(hdc, kPresetRenameText);
				SetBkColor(hdc, kPresetRenameFill);
				return reinterpret_cast<INT_PTR>(renameBrush_);
			}

			SetTextColor(hdc, kTextPrimary);
			SetBkColor(hdc, kInputFill);
			return reinterpret_cast<INT_PTR>(editBrush_);
		}
		case WM_CTLCOLORLISTBOX:
		{
			const HDC hdc = reinterpret_cast<HDC>(wideParameter);
			SetTextColor(hdc, kTextPrimary);
			SetBkColor(hdc, kInputFill);
			return reinterpret_cast<INT_PTR>(editBrush_);
		}
		case WM_PAINT:
			paintWindow();
			return 0;
		case WM_MOUSEMOVE:
		{
			if (!mouseTracked_) {
				mouseTracked_ = true;
				TRACKMOUSEEVENT trackMouse = {};
				trackMouse.cbSize = sizeof(trackMouse);
				trackMouse.dwFlags = TME_LEAVE;
				trackMouse.hwndTrack = nativeWindow_;
				TrackMouseEvent(&trackMouse);
			}

			const POINT point = { GET_X_LPARAM(longParameter), GET_Y_LPARAM(longParameter) };
			updateSecondaryTabHover(point);
			break;
		}
		case WM_MOUSELEAVE:
		{
			mouseTracked_ = false;
			POINT point = {};
			GetCursorPos(&point);
			ScreenToClient(nativeWindow_, &point);
			if (!isPointInsideSecondaryTabs(point)) {
				setSecondaryPagesVisible(currentTab == SettingsPage::PageExtra || currentTab == SettingsPage::PageOverlays);
			}
			break;
		}
		case WM_XBUTTONDOWN:
			if (activeHotkeyControl_) {
				const UINT key = HIWORD(wideParameter) == 1 ? VK_XBUTTON1 : VK_XBUTTON2;
				stashKeyHotkey(key);
			}
			break;
		case WM_MBUTTONDOWN:
			if (activeHotkeyControl_) {
				stashKeyHotkey(VK_MBUTTON);
			}
			break;
		case WM_RBUTTONDOWN:
			if (activeHotkeyControl_) {
				stashKeyHotkey(VK_RBUTTON);
			}
			break;
		case WM_LBUTTONDOWN:
			if (backdropOpen_)
			{
				const POINT point = { GET_X_LPARAM(longParameter), GET_Y_LPARAM(longParameter) };
				if (!pointHitsBackdropMenu(point)) {
					setBackdropMenuVisible(false);
				}
			}
			if (activeHotkeyControl_) {
				stashKeyHotkey(VK_LBUTTON);
			}
			break;
		case WM_KEYDOWN:
			if (activeHotkeyControl_) {
				stashKeyHotkey((UINT)wideParameter);
			}
			break;
		case WM_SYSKEYDOWN:
			if (activeHotkeyControl_ && (UINT)wideParameter == VK_MENU) {
				stashKeyHotkey((UINT)wideParameter);
			}
			break;
		case WM_PAD_EVENT:
			if (activeHotkeyControl_) {
				stashCtrlHotkey((UINT)wideParameter);
			}
			break;
		default:
			break;
		}
	}
	catch (const std::exception&)
	{
		postClose();
	}

	return DefWindowProc(id(), windowMessage, wideParameter, longParameter);
}

LRESULT CALLBACK PrefsPanel::panelCallback(
	const HWND idWin,
	const UINT uMsg,
	const WPARAM firstParam,
	const LPARAM secondParam,
	const UINT_PTR uIdSubclass,
	const DWORD_PTR dwRefData)
{
	PrefsPanel* settingsWindow = nullptr;
	if (overlayWindow_ != nullptr) {
		settingsWindow = overlayWindow_->prefsDialog;
	}

	int controlId = GetDlgCtrlID(idWin);
	if (dwRefData == CMD_WHEEL_PRESET || dwRefData == CMD_STREAK_PRESET) {
		controlId = static_cast<int>(dwRefData);
	}
	const bool isWheelPresetTextProxy = settingsWindow != nullptr &&
		controlId == CMD_WHEEL_PRESET &&
		idWin != settingsWindow->hndWhlPresets_;
	static DWORD wheelPresetLastClickTime = 0;
	static POINT wheelPresetLastClickPoint = {};

	switch (uMsg)
	{
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_XBUTTONDOWN:
		if (settingsWindow != nullptr)
		{
			const HWND settingsHwnd = settingsWindow->id();
			const bool isCtrlActive = settingsWindow->activeHotkeyControl_ != nullptr;
			if (settingsHwnd != nullptr && isCtrlActive)
			{
				SendMessage(settingsHwnd, uMsg, firstParam, secondParam);
				return 0;
			}
		}
		if (settingsWindow != nullptr && uMsg == WM_LBUTTONDOWN && controlId == CMD_WHEEL_SCROLLBAR)
		{
			settingsWindow->beginWheelItemsScrollDrag(GET_Y_LPARAM(secondParam));
			return 0;
		}
		if (settingsWindow != nullptr && uMsg == WM_LBUTTONDOWN && controlId == CMD_WHEEL_PRESET)
		{
			POINT comboPoint = { GET_X_LPARAM(secondParam), GET_Y_LPARAM(secondParam) };
			MapWindowPoints(idWin, settingsWindow->hndWhlPresets_, &comboPoint, 1);

			bool hitDropButton = false;
			RECT comboClientRect = {};
			if (GetClientRect(settingsWindow->hndWhlPresets_, &comboClientRect))
			{
				RECT buttonRect = comboClientRect;
				buttonRect.left = (std::max)(buttonRect.left + 24, buttonRect.right - 24);
				hitDropButton = PtInRect(&buttonRect, comboPoint) != FALSE;
			}

			if (hitDropButton)
			{
				wheelPresetLastClickTime = 0;
				break;
			}

			if (!hitDropButton)
			{
				const DWORD clickTime = GetTickCount();
				bool isDoubleClick = false;
				if (wheelPresetLastClickTime != 0)
				{
					const LONG deltaX = comboPoint.x > wheelPresetLastClickPoint.x
						? comboPoint.x - wheelPresetLastClickPoint.x
						: wheelPresetLastClickPoint.x - comboPoint.x;
					const LONG deltaY = comboPoint.y > wheelPresetLastClickPoint.y
						? comboPoint.y - wheelPresetLastClickPoint.y
						: wheelPresetLastClickPoint.y - comboPoint.y;
					isDoubleClick =
						clickTime - wheelPresetLastClickTime <= GetDoubleClickTime() &&
						deltaX <= GetSystemMetrics(SM_CXDOUBLECLK) &&
						deltaY <= GetSystemMetrics(SM_CYDOUBLECLK);
				}

				if (isDoubleClick)
				{
					wheelPresetLastClickTime = 0;
					SendMessageW(settingsWindow->hndWhlPresets_, CB_SHOWDROPDOWN, FALSE, 0);
					settingsWindow->beginWheelPresetRename();
					return 0;
				}

				wheelPresetLastClickTime = clickTime;
				wheelPresetLastClickPoint = comboPoint;
				SetFocus(settingsWindow->hndWhlPresets_);
				SendMessageW(settingsWindow->hndWhlPresets_, CB_SHOWDROPDOWN, TRUE, 0);
				return 0;
			}
		}
		if (settingsWindow != nullptr && uMsg == WM_LBUTTONDOWN && controlId == CMD_STREAK_SCROLL)
		{
			settingsWindow->beginStreakItemsScrollDrag(GET_Y_LPARAM(secondParam));
			return 0;
		}
		break;
	case WM_ERASEBKGND:
		if (settingsWindow != nullptr && !isWheelPresetTextProxy && (
			controlId == CMD_WHEEL_SCROLLBAR ||
			controlId == CMD_STREAK_SCROLL ||
			controlId == CMD_WHEEL_PRESET ||
			controlId == CMD_STREAK_PRESET)) {
			return 1;
		}
		break;
	case WM_GETDLGCODE:
		if (controlId == CMD_WHEEL_RENAME) {
			return DLGC_WANTALLKEYS;
		}
		break;
	case WM_PAINT:
		if (settingsWindow != nullptr && controlId == CMD_WHEEL_SCROLLBAR)
		{
			PAINTSTRUCT paintStruct = {};
			const HDC hdc = BeginPaint(idWin, &paintStruct);
			settingsWindow->drawWheelItemsScrollbar(hdc);
			EndPaint(idWin, &paintStruct);
			return 0;
		}
		if (settingsWindow != nullptr && controlId == CMD_STREAK_SCROLL)
		{
			PAINTSTRUCT paintStruct = {};
			const HDC hdc = BeginPaint(idWin, &paintStruct);
			settingsWindow->drawStreakItemsScrollbar(hdc);
			EndPaint(idWin, &paintStruct);
			return 0;
		}
		if (settingsWindow != nullptr && !isWheelPresetTextProxy && (controlId == CMD_WHEEL_PRESET || controlId == CMD_STREAK_PRESET))
		{
			PAINTSTRUCT paintStruct = {};
			const HDC hdc = BeginPaint(idWin, &paintStruct);
			settingsWindow->paintThemePresetCombo(idWin, hdc);
			EndPaint(idWin, &paintStruct);
			return 0;
		}
		break;
	case WM_KEYDOWN:
		if (settingsWindow != nullptr && controlId == CMD_WHEEL_RENAME)
		{
			if (firstParam == VK_RETURN)
			{
				settingsWindow->commitWheelPresetRename(true);
				return 0;
			}

			if (firstParam == VK_ESCAPE)
			{
				settingsWindow->commitWheelPresetRename(false);
				return 0;
			}
		}
		if (settingsWindow != nullptr && controlId == CMD_WHEEL_ITEMS)
		{
			const LRESULT result = DefSubclassProc(idWin, uMsg, firstParam, secondParam);
			settingsWindow->syncWheelItemsScrollbar();
			return result;
		}
		if (settingsWindow != nullptr && controlId == CMD_STREAK_ITEMS)
		{
			const LRESULT result = DefSubclassProc(idWin, uMsg, firstParam, secondParam);
			settingsWindow->syncStreakItemsScrollbar();
			return result;
		}
		break;
	case WM_MOUSEWHEEL:
		if (settingsWindow != nullptr && controlId == CMD_WHEEL_ITEMS)
		{
			const LRESULT result = DefSubclassProc(idWin, uMsg, firstParam, secondParam);
			settingsWindow->syncWheelItemsScrollbar();
			return result;
		}
		if (settingsWindow != nullptr && controlId == CMD_STREAK_ITEMS)
		{
			const LRESULT result = DefSubclassProc(idWin, uMsg, firstParam, secondParam);
			settingsWindow->syncStreakItemsScrollbar();
			return result;
		}
		if (settingsWindow != nullptr && controlId == CMD_WHEEL_SCROLLBAR)
		{
			const int wheelDelta = GET_WHEEL_DELTA_WPARAM(firstParam);
			const int wheelStep = wheelDelta > 0 ? -3 : 3;
			const int firstVisibleLine = settingsWindow->hndWhlItems_ == nullptr
				? 0
				: static_cast<int>(SendMessageW(settingsWindow->hndWhlItems_, EM_GETFIRSTVISIBLELINE, 0, 0));
			settingsWindow->setWheelItemsTopLine(firstVisibleLine + wheelStep);
			return 0;
		}
		if (settingsWindow != nullptr && controlId == CMD_STREAK_SCROLL)
		{
			const int wheelDelta = GET_WHEEL_DELTA_WPARAM(firstParam);
			const int wheelStep = wheelDelta > 0 ? -3 : 3;
			const int firstVisibleLine = settingsWindow->hndStkItems_ == nullptr
				? 0
				: static_cast<int>(SendMessageW(settingsWindow->hndStkItems_, EM_GETFIRSTVISIBLELINE, 0, 0));
			settingsWindow->setStreakItemsTopLine(firstVisibleLine + wheelStep);
			return 0;
		}
		break;
	case WM_VSCROLL:
		if (settingsWindow != nullptr && controlId == CMD_WHEEL_ITEMS)
		{
			const LRESULT result = DefSubclassProc(idWin, uMsg, firstParam, secondParam);
			settingsWindow->syncWheelItemsScrollbar();
			return result;
		}
		if (settingsWindow != nullptr && controlId == CMD_STREAK_ITEMS)
		{
			const LRESULT result = DefSubclassProc(idWin, uMsg, firstParam, secondParam);
			settingsWindow->syncStreakItemsScrollbar();
			return result;
		}
		break;
	case WM_LBUTTONDBLCLK:
		if (settingsWindow != nullptr && controlId == CMD_WHEEL_PRESET)
		{
			settingsWindow->beginWheelPresetRename();
			return 0;
		}
		if (settingsWindow != nullptr && controlId == CMD_WHEEL_SCROLLBAR)
		{
			settingsWindow->beginWheelItemsScrollDrag(GET_Y_LPARAM(secondParam));
			return 0;
		}
		if (settingsWindow != nullptr && controlId == CMD_STREAK_SCROLL)
		{
			settingsWindow->beginStreakItemsScrollDrag(GET_Y_LPARAM(secondParam));
			return 0;
		}
		SendMessage(idWin, WM_LBUTTONDOWN, firstParam, secondParam);
		return 0;
	case WM_LBUTTONUP:
		if (settingsWindow != nullptr && controlId == CMD_WHEEL_SCROLLBAR)
		{
			settingsWindow->endWheelItemsScrollDrag();
			return 0;
		}
		if (settingsWindow != nullptr && controlId == CMD_STREAK_SCROLL)
		{
			settingsWindow->endStreakItemsScrollDrag();
			return 0;
		}
		if (settingsWindow != nullptr && controlId == CMD_WHEEL_ITEMS)
		{
			const LRESULT result = DefSubclassProc(idWin, uMsg, firstParam, secondParam);
			settingsWindow->syncWheelItemsScrollbar();
			return result;
		}
		if (settingsWindow != nullptr && controlId == CMD_STREAK_ITEMS)
		{
			const LRESULT result = DefSubclassProc(idWin, uMsg, firstParam, secondParam);
			settingsWindow->syncStreakItemsScrollbar();
			return result;
		}
		break;
	case WM_MOUSEMOVE:
		if (settingsWindow != nullptr && (controlId == CMD_PAGE_EXTRA || controlId == CMD_PAGE_OVERLAYS))
		{
			POINT point = { GET_X_LPARAM(secondParam), GET_Y_LPARAM(secondParam) };
			MapWindowPoints(idWin, settingsWindow->id(), &point, 1);
			settingsWindow->updateSecondaryTabHover(point);
		}
		if (settingsWindow != nullptr && controlId == CMD_WHEEL_SCROLLBAR)
		{
			if ((firstParam & MK_LBUTTON) != 0) {
				settingsWindow->updateWheelItemsScrollDrag(GET_Y_LPARAM(secondParam));
			}
			return 0;
		}
		if (settingsWindow != nullptr && controlId == CMD_STREAK_SCROLL)
		{
			if ((firstParam & MK_LBUTTON) != 0) {
				settingsWindow->updateStreakItemsScrollDrag(GET_Y_LPARAM(secondParam));
			}
			return 0;
		}
		break;
	case WM_CAPTURECHANGED:
		if (settingsWindow != nullptr && controlId == CMD_WHEEL_SCROLLBAR)
		{
			settingsWindow->endWheelItemsScrollDrag();
			return 0;
		}
		if (settingsWindow != nullptr && controlId == CMD_STREAK_SCROLL)
		{
			settingsWindow->endStreakItemsScrollDrag();
			return 0;
		}
		break;
	case WM_SETTEXT:
		if (settingsWindow != nullptr && controlId == CMD_WHEEL_ITEMS)
		{
			const LRESULT result = DefSubclassProc(idWin, uMsg, firstParam, secondParam);
			settingsWindow->syncWheelItemsScrollbar();
			return result;
		}
		if (settingsWindow != nullptr && controlId == CMD_STREAK_ITEMS)
		{
			const LRESULT result = DefSubclassProc(idWin, uMsg, firstParam, secondParam);
			settingsWindow->syncStreakItemsScrollbar();
			return result;
		}
		break;
	default:
		break;
	}

	return DefSubclassProc(idWin, uMsg, firstParam, secondParam);
}


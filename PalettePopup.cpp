#include <Windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <algorithm>
#include <cwchar>
#include <string>
#include "AppConfig.h"
#include "PalettePopup.h"
#include "EntryPoint.h"

#pragma comment(lib, "gdiplus.lib")

namespace
{
	constexpr int kThemeStartIndex = 25;
	constexpr int kMyColorStartIndex = 75;
	constexpr int kThemeRows = 5;
	constexpr int kThemeCols = 10;
	constexpr int kMyRows = 3;
	constexpr int kMyCols = 10;

	constexpr int kCardLeft = 8;
	constexpr int kCardTop = 8;
	constexpr int kCardRight = DLG_TINT_W - 8;
	constexpr int kCardBottom = DLG_TINT_H - 8;
	constexpr int kTitleX = 30;
	constexpr int kThemeLeft = 30;
	constexpr int kThemeTop = 132;
	constexpr int kThemeSwatchW = 43;
	constexpr int kThemeSwatchH = 25;
	constexpr int kThemeRowGap = 12;
	constexpr int kCircleStartX = 46;
	constexpr int kCircleStartY = 386;
	constexpr int kCircleGapX = 44;
	constexpr int kCircleGapY = 42;
	constexpr int kCircleRadius = 16;
	constexpr COLORREF kPickerCard = RGB(22, 30, 42);
	constexpr COLORREF kPickerBorder = RGB(52, 72, 98);
	constexpr COLORREF kPickerText = RGB(228, 235, 245);
	constexpr COLORREF kPickerMutedText = RGB(140, 162, 185);
	constexpr COLORREF kPickerAccent = RGB(30, 155, 235);

	COLORREF colorFromBrush(const HBRUSH brush)
	{
		LOGBRUSH logBrush = {};
		GetObject(brush, sizeof(logBrush), &logBrush);
		return logBrush.lbColor;
	}

	HFONT createUiFont(const int size, const int weight = FW_NORMAL)
	{
		return CreateFontW(
			-size,
			0,
			0,
			0,
			weight,
			FALSE,
			FALSE,
			FALSE,
			DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE,
			L"Segoe UI");
	}

	void drawText(HDC hdc, const wchar_t* text, RECT rect, const COLORREF color, const UINT format)
	{
		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, color);
		DrawTextW(hdc, text, -1, &rect, format);
	}

	void fillSolidRect(HDC hdc, const RECT& rect, const COLORREF color)
	{
		const HBRUSH brush = CreateSolidBrush(color);
		FillRect(hdc, &rect, brush);
		DeleteObject(brush);
	}

	void drawRoundRect(HDC hdc, const RECT& rect, const COLORREF fill, const COLORREF border, const int radius)
	{
		const HBRUSH brush = CreateSolidBrush(fill);
		const HPEN pen = CreatePen(PS_SOLID, 1, border);
		const HGDIOBJ oldBrush = SelectObject(hdc, brush);
		const HGDIOBJ oldPen = SelectObject(hdc, pen);

		RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);

		SelectObject(hdc, oldPen);
		SelectObject(hdc, oldBrush);
		DeleteObject(pen);
		DeleteObject(brush);
	}

	void drawWindowBorder(HDC hdc, const RECT& clientRect)
	{
		Gdiplus::Graphics graphics(hdc);
		graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
		graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

		Gdiplus::Pen borderPen(Gdiplus::Color(255, 104, 196, 244), 1.7f);
		Gdiplus::RectF borderRect(
			static_cast<Gdiplus::REAL>(clientRect.left + 2.5f),
			static_cast<Gdiplus::REAL>(clientRect.top + 2.5f),
			static_cast<Gdiplus::REAL>((clientRect.right - clientRect.left) - 5.0f),
			static_cast<Gdiplus::REAL>((clientRect.bottom - clientRect.top) - 5.0f));
		graphics.DrawArc(&borderPen, borderRect.X, borderRect.Y, 24.0f, 24.0f, 180.0f, 90.0f);
		graphics.DrawArc(&borderPen, borderRect.X + borderRect.Width - 24.0f, borderRect.Y, 24.0f, 24.0f, 270.0f, 90.0f);
		graphics.DrawArc(&borderPen, borderRect.X + borderRect.Width - 24.0f, borderRect.Y + borderRect.Height - 24.0f, 24.0f, 24.0f, 0.0f, 90.0f);
		graphics.DrawArc(&borderPen, borderRect.X, borderRect.Y + borderRect.Height - 24.0f, 24.0f, 24.0f, 90.0f, 90.0f);
		graphics.DrawLine(&borderPen, borderRect.X + 12.0f, borderRect.Y, borderRect.X + borderRect.Width - 12.0f, borderRect.Y);
		graphics.DrawLine(&borderPen, borderRect.X + borderRect.Width, borderRect.Y + 12.0f, borderRect.X + borderRect.Width, borderRect.Y + borderRect.Height - 12.0f);
		graphics.DrawLine(&borderPen, borderRect.X + borderRect.Width - 12.0f, borderRect.Y + borderRect.Height, borderRect.X + 12.0f, borderRect.Y + borderRect.Height);
		graphics.DrawLine(&borderPen, borderRect.X, borderRect.Y + borderRect.Height - 12.0f, borderRect.X, borderRect.Y + 12.0f);
	}

	void drawCircle(Gdiplus::Graphics& graphics, const int centerX, const int centerY, const int radius, const COLORREF fill)
	{
		Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(fill), GetGValue(fill), GetBValue(fill)));
		graphics.FillEllipse(
			&brush,
			static_cast<Gdiplus::REAL>(centerX - radius),
			static_cast<Gdiplus::REAL>(centerY - radius),
			static_cast<Gdiplus::REAL>(radius * 2),
			static_cast<Gdiplus::REAL>(radius * 2));
	}

	void drawSelectionRing(HDC hdc, Gdiplus::Graphics* graphics, const RECT& rect, const bool circle)
	{
		if (circle && graphics != nullptr)
		{
			Gdiplus::Pen outerPen(Gdiplus::Color(255, 255, 255, 255), 3.0f);
			Gdiplus::Pen innerPen(Gdiplus::Color(255, 38, 136, 235), 2.0f);
			graphics->DrawEllipse(
				&outerPen,
				static_cast<Gdiplus::REAL>(rect.left - 4),
				static_cast<Gdiplus::REAL>(rect.top - 4),
				static_cast<Gdiplus::REAL>((rect.right - rect.left) + 8),
				static_cast<Gdiplus::REAL>((rect.bottom - rect.top) + 8));
			graphics->DrawEllipse(
				&innerPen,
				static_cast<Gdiplus::REAL>(rect.left - 2),
				static_cast<Gdiplus::REAL>(rect.top - 2),
				static_cast<Gdiplus::REAL>((rect.right - rect.left) + 4),
				static_cast<Gdiplus::REAL>((rect.bottom - rect.top) + 4));
			return;
		}

		const HPEN outerPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
		const HPEN innerPen = CreatePen(PS_SOLID, 2, RGB(38, 136, 235));
		const HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
		const HGDIOBJ oldPen = SelectObject(hdc, outerPen);

		if (circle) {
			Ellipse(hdc, rect.left - 4, rect.top - 4, rect.right + 4, rect.bottom + 4);
		}
		else {
			Rectangle(hdc, rect.left - 2, rect.top - 2, rect.right + 2, rect.bottom + 2);
		}

		SelectObject(hdc, innerPen);
		if (circle) {
			Ellipse(hdc, rect.left - 2, rect.top - 2, rect.right + 2, rect.bottom + 2);
		}
		else {
			Rectangle(hdc, rect.left - 1, rect.top - 1, rect.right + 1, rect.bottom + 1);
		}

		SelectObject(hdc, oldPen);
		SelectObject(hdc, oldBrush);
		DeleteObject(innerPen);
		DeleteObject(outerPen);
	}

	bool pointInRect(const int x, const int y, const RECT& rect)
	{
		return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
	}

	RECT closeButtonRect()
	{
		return RECT{ DLG_TINT_W - 60, 30, DLG_TINT_W - 36, 56 };
	}

	RECT themeSwatchRect(const int row, const int col)
	{
		const int left = kThemeLeft + col * kThemeSwatchW;
		const int top = kThemeTop + row * (kThemeSwatchH + kThemeRowGap);
		return RECT{ left, top, left + kThemeSwatchW, top + kThemeSwatchH };
	}

	RECT myColorCircleRect(const int row, const int col)
	{
		const int centerX = kCircleStartX + col * kCircleGapX;
		const int centerY = kCircleStartY + row * kCircleGapY;
		return RECT{ centerX - kCircleRadius, centerY - kCircleRadius, centerX + kCircleRadius, centerY + kCircleRadius };
	}

	std::wstring hexFromColor(const COLORREF color)
	{
		wchar_t text[16] = {};
		swprintf_s(text, L"# %02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));
		return text;
	}
}

void PalettePopup::buildSwatchGrid()
{
	swatchIdx_ = getCurrentSelection();
}

int PalettePopup::getCurrentSelection() const
{
	if (parentCfg == nullptr) {
		return 0;
	}

	switch (originCtrlId)
	{
	case CMD_CLR_TIMER:
		return parentCfg->tintSelection.timerOneColor;
	case CMD_CLR_SEL_TIMER:
		return parentCfg->tintSelection.timerTwoColor;
	case CMD_CLR_URGENT:
		return parentCfg->tintSelection.urgentColor;
	case CMD_CLR_BACKDROP:
		return parentCfg->tintSelection.backdropColor;
	default:
		return 0;
	}
}

void PalettePopup::applySelectedColor(const int colorIndex)
{
	if (parentCfg == nullptr || colorIndex < 0 || colorIndex >= TINT_PALETTE_SZ) {
		return;
	}

	switch (originCtrlId)
	{
	case CMD_CLR_TIMER:
		parentCfg->tintSelection.timerOneColor = colorIndex;
		break;
	case CMD_CLR_SEL_TIMER:
		parentCfg->tintSelection.timerTwoColor = colorIndex;
		break;
	case CMD_CLR_URGENT:
		parentCfg->tintSelection.urgentColor = colorIndex;
		break;
	case CMD_CLR_BACKDROP:
		parentCfg->tintSelection.backdropColor = colorIndex;
		break;
	default:
		break;
	}

	swatchIdx_ = colorIndex;
	const HWND owner = GetWindow(nativeWindow_, GW_OWNER);
	if (owner != nullptr) {
		RedrawWindow(owner, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	}
	RedrawWindow(nativeWindow_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
}

void PalettePopup::paintSwatches() const
{
	PAINTSTRUCT ps = {};
	const HDC hdc = BeginPaint(nativeWindow_, &ps);

	RECT clientRect = {};
	GetClientRect(nativeWindow_, &clientRect);
	fillSolidRect(hdc, clientRect, kPickerCard);
	drawWindowBorder(hdc, clientRect);

	const HFONT titleFont = createUiFont(20, FW_BOLD);
	const HFONT labelFont = createUiFont(13, FW_BOLD);
	const HFONT bodyFont = createUiFont(13, FW_NORMAL);
	const HFONT hexFont = createUiFont(13, FW_BOLD);
	const HGDIOBJ oldFont = SelectObject(hdc, titleFont);

	drawText(hdc, L"Color Picker", RECT{ kTitleX, 36, 260, 64 }, kPickerText, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

	const RECT closeRect = closeButtonRect();
	drawRoundRect(hdc, closeRect, RGB(14, 20, 30), kPickerBorder, 2);
	SelectObject(hdc, bodyFont);
	drawText(hdc, L"x", closeRect, kPickerText, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

	SelectObject(hdc, labelFont);
	drawText(hdc, L"Theme color", RECT{ 30, 100, 160, 120 }, kPickerText, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
	drawText(hdc, L"Edit", RECT{ 430, 100, 462, 120 }, kPickerAccent, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

	for (int row = 0; row < kThemeRows; ++row)
	{
		for (int col = 0; col < kThemeCols; ++col)
		{
			const int colorIndex = kThemeStartIndex + row * kThemeCols + col;
			const RECT swatchRect = themeSwatchRect(row, col);
			fillSolidRect(hdc, swatchRect, colorFromBrush(paletteBrushes_[colorIndex]));
			if (swatchIdx_ == colorIndex) {
				drawSelectionRing(hdc, nullptr, swatchRect, false);
			}
		}
	}

	SelectObject(hdc, labelFont);
	drawText(hdc, L"My color", RECT{ 30, 337, 160, 358 }, kPickerText, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
	drawText(hdc, L"+ Add", RECT{ 417, 337, 462, 358 }, kPickerAccent, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

	Gdiplus::Graphics circleGraphics(hdc);
	circleGraphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	circleGraphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

	for (int row = 0; row < kMyRows; ++row)
	{
		for (int col = 0; col < kMyCols; ++col)
		{
			const int colorIndex = kMyColorStartIndex + row * kMyCols + col;
			const RECT circleRect = myColorCircleRect(row, col);
			const int centerX = (circleRect.left + circleRect.right) / 2;
			const int centerY = (circleRect.top + circleRect.bottom) / 2;
			drawCircle(circleGraphics, centerX, centerY, kCircleRadius, colorFromBrush(paletteBrushes_[colorIndex]));
			if (swatchIdx_ == colorIndex) {
				drawSelectionRing(hdc, &circleGraphics, circleRect, true);
			}
		}
	}

	const COLORREF previewColor = colorFromBrush(paletteBrushes_[(std::max)(0, (std::min)(static_cast<int>(TINT_PALETTE_SZ) - 1, swatchIdx_))]);
	const std::wstring hexText = hexFromColor(previewColor);
	SelectObject(hdc, hexFont);
	drawText(hdc, hexText.c_str(), RECT{ 394, 506, 462, 526 }, kPickerMutedText, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

	SelectObject(hdc, oldFont);
	DeleteObject(hexFont);
	DeleteObject(bodyFont);
	DeleteObject(labelFont);
	DeleteObject(titleFont);

	EndPaint(nativeWindow_, &ps);
}

void PalettePopup::handleSwatchClick(const int x, const int y)
{
	if (pointInRect(x, y, closeButtonRect()))
	{
		DestroyWindow(nativeWindow_);
		return;
	}

	for (int row = 0; row < kThemeRows; ++row)
	{
		for (int col = 0; col < kThemeCols; ++col)
		{
			if (pointInRect(x, y, themeSwatchRect(row, col)))
			{
				applySelectedColor(kThemeStartIndex + row * kThemeCols + col);
				return;
			}
		}
	}

	for (int row = 0; row < kMyRows; ++row)
	{
		for (int col = 0; col < kMyCols; ++col)
		{
			const RECT circleRect = myColorCircleRect(row, col);
			const int centerX = (circleRect.left + circleRect.right) / 2;
			const int centerY = (circleRect.top + circleRect.bottom) / 2;
			const int dx = x - centerX;
			const int dy = y - centerY;
			if ((dx * dx) + (dy * dy) <= kCircleRadius * kCircleRadius)
			{
				applySelectedColor(kMyColorStartIndex + row * kMyCols + col);
				return;
			}
		}
	}
}

LRESULT PalettePopup::processMessage(UINT windowMessage, WPARAM wideParameter, LPARAM longParameter)
{
	switch (windowMessage)
	{
	case WM_CREATE:
		buildSwatchGrid();
		return 0;
	case WM_DESTROY:
		nativeWindow_ = nullptr;
		return 0;
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
		paintSwatches();
		return 0;
	case WM_LBUTTONDOWN:
		handleSwatchClick(GET_X_LPARAM(longParameter), GET_Y_LPARAM(longParameter));
		return 0;
	case WM_KEYDOWN:
		if (wideParameter == VK_ESCAPE) {
			DestroyWindow(nativeWindow_);
			return 0;
		}
		break;
	case WM_NCHITTEST:
	{
		const LRESULT hit = DefWindowProc(nativeWindow_, windowMessage, wideParameter, longParameter);
		if (hit == HTCLIENT)
		{
			POINT point = { GET_X_LPARAM(longParameter), GET_Y_LPARAM(longParameter) };
			ScreenToClient(nativeWindow_, &point);
			if (point.y >= 12 && point.y <= 72 && !pointInRect(point.x, point.y, closeButtonRect())) {
				return HTCAPTION;
			}
		}
		return hit;
	}
	default:
		break;
	}

	return DefWindowProc(id(), windowMessage, wideParameter, longParameter);
}

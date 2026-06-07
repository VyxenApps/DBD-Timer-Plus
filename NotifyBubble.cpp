#include "NotifyBubble.h"
#include "AppConfig.h"
#include <algorithm>

#pragma comment(lib, "gdiplus.lib")

namespace
{
	constexpr int kReferenceScreenWidth = 1920;
	constexpr int kReferenceScreenHeight = 1080;
	constexpr int kChatToxicLeft = 1464;
	constexpr int kChatToxicTop = 626;
	constexpr int kChatToxicWidth = 372;
	constexpr int kChatToxicHeight = 246;
	constexpr COLORREF kOverlayFill = RGB(19, 26, 38);
	constexpr COLORREF kOverlayBorder = RGB(48, 67, 92);
	constexpr COLORREF kOverlayAccent = RGB(136, 219, 255);
	constexpr UINT_PTR kAutoHideTimerId = 1;
}

NotifyBubble::~NotifyBubble()
{
	releaseBgImage();
	if (gdiToken_ != 0) {
		Gdiplus::GdiplusShutdown(gdiToken_);
		gdiToken_ = 0;
	}

	if (nativeWindow_ != nullptr) {
		DestroyWindow(nativeWindow_);
	}
}

RECT NotifyBubble::computeOverlayBounds() const
{
	const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	const int left = MulDiv(kChatToxicLeft, screenWidth, kReferenceScreenWidth);
	const int top = MulDiv(kChatToxicTop, screenHeight, kReferenceScreenHeight);
	const int width = MulDiv(kChatToxicWidth, screenWidth, kReferenceScreenWidth);
	const int height = MulDiv(kChatToxicHeight, screenHeight, kReferenceScreenHeight);

	return RECT{ left, top, left + width, top + height };
}

void NotifyBubble::createid()
{
	if (nativeWindow_ != nullptr) {
		return;
	}

	const RECT rect = computeOverlayBounds();
	spawn(
		rect.left,
		rect.top,
		rect.right - rect.left,
		rect.bottom - rect.top,
		WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
		WS_POPUP,
		nullptr,
		L"Chat Toxic Overlay");

	if (nativeWindow_ != nullptr)
	{
		SetLayeredWindowAttributes(nativeWindow_, 0, 255, LWA_ALPHA);
		maskWindow();
	}
}

void NotifyBubble::releaseBgImage()
{
	if (bgImg_ != nullptr)
	{
		delete bgImg_;
		bgImg_ = nullptr;
	}
	curImgPath_.clear();
}

void NotifyBubble::loadBgImage()
{
	if (overlayCfg_.toxicChatImg == curImgPath_) {
		return;
	}

	releaseBgImage();
	if (overlayCfg_.toxicChatImg.empty()) {
		return;
	}

	if (gdiToken_ == 0)
	{
		Gdiplus::GdiplusStartupInput startupInput;
		if (Gdiplus::GdiplusStartup(&gdiToken_, &startupInput, nullptr) != Gdiplus::Ok) {
			gdiToken_ = 0;
			return;
		}
	}

	Gdiplus::Image* image = Gdiplus::Image::FromFile(overlayCfg_.toxicChatImg.c_str(), FALSE);
	if (image == nullptr || image->GetLastStatus() != Gdiplus::Ok)
	{
		delete image;
		return;
	}

	bgImg_ = image;
	curImgPath_ = overlayCfg_.toxicChatImg;
}

void NotifyBubble::maskWindow() const
{
	if (nativeWindow_ == nullptr) {
		return;
	}

	RECT clientRect = {};
	GetClientRect(nativeWindow_, &clientRect);
	HRGN roundedRegion = CreateRoundRectRgn(
		clientRect.left,
		clientRect.top,
		clientRect.right + 1,
		clientRect.bottom + 1,
		34,
		34);

	if (roundedRegion != nullptr) {
		SetWindowRgn(nativeWindow_, roundedRegion, TRUE);
	}
}

void NotifyBubble::armAutoHide(const int durationMillis) const
{
	if (nativeWindow_ == nullptr) {
		return;
	}

	const int clampedDuration = (std::max)(1000, durationMillis);
	KillTimer(nativeWindow_, kAutoHideTimerId);
	SetTimer(nativeWindow_, kAutoHideTimerId, static_cast<UINT>(clampedDuration), nullptr);
}

void NotifyBubble::setVisible(const bool enabled)
{
	if (!enabled && autoHideActive_) {
		return;
	}

	visible_ = enabled;
	if (!visible_)
	{
		if (nativeWindow_ != nullptr) {
			KillTimer(nativeWindow_, kAutoHideTimerId);
		}
		if (nativeWindow_ != nullptr) {
			ShowWindow(nativeWindow_, SW_HIDE);
		}
		return;
	}

	createid();
	if (nativeWindow_ == nullptr) {
		return;
	}

	loadBgImage();
	const RECT rect = computeOverlayBounds();
	SetWindowPos(
		nativeWindow_,
		HWND_TOPMOST,
		rect.left,
		rect.top,
		rect.right - rect.left,
		rect.bottom - rect.top,
		SWP_NOACTIVATE | SWP_SHOWWINDOW);
	maskWindow();
	if (overlayCfg_.toxicChatTimed) {
		autoHideActive_ = true;
		armAutoHide(overlayCfg_.toxicChatDurationMs);
	}
	RedrawWindow(nativeWindow_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
}

void NotifyBubble::flash(const int durationMillis)
{
	if (autoHideActive_) {
		return;
	}

	const bool originalTimedSetting = overlayCfg_.toxicChatTimed;
	overlayCfg_.toxicChatTimed = false;
	setVisible(true);
	overlayCfg_.toxicChatTimed = originalTimedSetting;
	autoHideActive_ = true;
	armAutoHide(durationMillis);
}

void NotifyBubble::onAutoHide()
{
	autoHideActive_ = false;
	visible_ = false;
	if (nativeWindow_ != nullptr) {
		KillTimer(nativeWindow_, kAutoHideTimerId);
		ShowWindow(nativeWindow_, SW_HIDE);
	}
}

void NotifyBubble::applyConfig(const NotifyProfile& settings)
{
	overlayCfg_ = settings;
	overlayCfg_.toxicChatEnabled = false;
	loadBgImage();
	setVisible(false);
}

LRESULT NotifyBubble::processMessage(UINT windowMessage, WPARAM wideParameter, LPARAM longParameter)
{
	switch (windowMessage)
	{
	case WM_ERASEBKGND:
		return 1;
	case WM_NCHITTEST:
		return HTTRANSPARENT;
	case WM_MOUSEACTIVATE:
		return MA_NOACTIVATE;
	case WM_TIMER:
		if (wideParameter == kAutoHideTimerId)
		{
			onAutoHide();
			return 0;
		}
		break;
	case WM_SIZE:
		maskWindow();
		return 0;
	case WM_DESTROY:
		nativeWindow_ = nullptr;
		return 0;
	case WM_PAINT:
	{
		PAINTSTRUCT paintStruct = {};
		const HDC hdc = BeginPaint(nativeWindow_, &paintStruct);
		RECT clientRect = {};
		GetClientRect(nativeWindow_, &clientRect);

		const HBRUSH fillBrush = CreateSolidBrush(kOverlayFill);
		FillRect(hdc, &clientRect, fillBrush);

		if (bgImg_ != nullptr)
		{
			Gdiplus::Graphics graphics(hdc);
			graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
			graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
			graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);

			const float clientWidth = static_cast<float>(clientRect.right - clientRect.left);
			const float clientHeight = static_cast<float>(clientRect.bottom - clientRect.top);
			const float imageWidth = static_cast<float>(bgImg_->GetWidth());
			const float imageHeight = static_cast<float>(bgImg_->GetHeight());
			const float scale = (std::max)(clientWidth / imageWidth, clientHeight / imageHeight);
			const float drawWidth = imageWidth * scale;
			const float drawHeight = imageHeight * scale;
			const float drawX = (clientWidth - drawWidth) * 0.5f;
			const float drawY = (clientHeight - drawHeight) * 0.5f;

			graphics.DrawImage(bgImg_, Gdiplus::RectF(drawX, drawY, drawWidth, drawHeight));

			Gdiplus::SolidBrush shadeBrush(Gdiplus::Color(48, 19, 26, 38));
			graphics.FillRectangle(&shadeBrush, Gdiplus::RectF(0, 0, clientWidth, clientHeight));
		}

		const HPEN borderPen = CreatePen(PS_SOLID, 2, kOverlayBorder);
		const HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
		const HGDIOBJ oldPen = SelectObject(hdc, borderPen);
		RoundRect(hdc, clientRect.left, clientRect.top, clientRect.right, clientRect.bottom, 34, 34);

		const HPEN accentPen = CreatePen(PS_SOLID, 1, kOverlayAccent);
		SelectObject(hdc, accentPen);
		RECT innerRect = clientRect;
		InflateRect(&innerRect, -4, -4);
		RoundRect(hdc, innerRect.left, innerRect.top, innerRect.right, innerRect.bottom, 26, 26);

		SelectObject(hdc, oldBrush);
		SelectObject(hdc, oldPen);
		DeleteObject(accentPen);
		DeleteObject(borderPen);
		DeleteObject(fillBrush);

		EndPaint(nativeWindow_, &paintStruct);
		return 0;
	}
	default:
		break;
	}

	return DefWindowProc(nativeWindow_, windowMessage, wideParameter, longParameter);
}

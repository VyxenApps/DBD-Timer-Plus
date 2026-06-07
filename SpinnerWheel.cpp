#include "SpinnerWheel.h"
#include <CommCtrl.h>
#include <algorithm>
#include <cmath>
#include <mmsystem.h>
#include <random>
#include <sstream>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")

namespace
{
	constexpr int kSpinButtonId = 201;
	constexpr int kChromaButtonId = 202;
	constexpr UINT_PTR kSpinTimerId = 1;
	constexpr UINT kSpinTimerIntervalMs = 16;
	constexpr const wchar_t* kSpinSoundAlias = L"dbd_wheel_spin_sound";
	constexpr const wchar_t* kEndSoundAlias = L"dbd_wheel_end_sound";
	constexpr const wchar_t* kSpinSoundRelativePath = L"Audio\\Ruleta giro.mp3";
	constexpr const wchar_t* kEndSoundRelativePath = L"Audio\\wd-sound-fx-end.mp3";
	constexpr COLORREF kChromaKeyGreen = RGB(0, 255, 0);
	constexpr COLORREF kWheelBackground = RGB(11, 16, 24);
	constexpr COLORREF kWheelCard = RGB(20, 28, 41);
	constexpr COLORREF kWheelAccent = RGB(33, 159, 240);
	constexpr COLORREF kWheelGold = RGB(255, 197, 61);
	constexpr COLORREF kWheelText = RGB(18, 22, 28);
	constexpr COLORREF kWheelTextMuted = RGB(210, 219, 228);
	constexpr COLORREF kSliceColorBlue = RGB(48, 156, 255);
	constexpr COLORREF kSliceColorWhite = RGB(248, 248, 248);
	constexpr COLORREF kSliceColorRed = RGB(255, 61, 61);
	constexpr COLORREF kSliceColorPink = RGB(255, 84, 172);
	constexpr COLORREF kSliceColorGreen = RGB(44, 227, 61);
	constexpr COLORREF kSliceColorPurple = RGB(133, 36, 232);
	constexpr COLORREF kSliceColorYellow = RGB(255, 208, 61);

	COLORREF adjustColor(const COLORREF color, const int amount)
	{
		const int red = (std::max)(0, (std::min)(255, GetRValue(color) + amount));
		const int green = (std::max)(0, (std::min)(255, GetGValue(color) + amount));
		const int blue = (std::max)(0, (std::min)(255, GetBValue(color) + amount));
		return RGB(red, green, blue);
	}

	std::wstring getModuleDirectory()
	{
		wchar_t modulePath[MAX_PATH] = {};
		const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
		if (length == 0 || length >= MAX_PATH) {
			return L"";
		}

		std::wstring directory(modulePath, length);
		const size_t separator = directory.find_last_of(L"\\/");
		return separator == std::wstring::npos ? L"" : directory.substr(0, separator);
	}

	std::wstring combinePath(const std::wstring& directory, const std::wstring& relativePath)
	{
		if (directory.empty()) {
			return relativePath;
		}

		const wchar_t lastChar = directory[directory.size() - 1];
		return (lastChar == L'\\' || lastChar == L'/')
			? directory + relativePath
			: directory + L"\\" + relativePath;
	}

	std::wstring resolveAudioPath(const std::wstring& relativePath)
	{
		const std::wstring modulePath = combinePath(getModuleDirectory(), relativePath);
		if (GetFileAttributesW(modulePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
			return modulePath;
		}

		wchar_t currentDirectory[MAX_PATH] = {};
		if (GetCurrentDirectoryW(MAX_PATH, currentDirectory) > 0)
		{
			const std::wstring currentPath = combinePath(currentDirectory, relativePath);
			if (GetFileAttributesW(currentPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
				return currentPath;
			}
		}

		return modulePath;
	}

	void stopWheelAudio(const wchar_t* alias)
	{
		if (alias == nullptr) {
			return;
		}

		std::wstring stopCommand = L"stop ";
		stopCommand += alias;
		mciSendStringW(stopCommand.c_str(), nullptr, 0, nullptr);

		std::wstring closeCommand = L"close ";
		closeCommand += alias;
		mciSendStringW(closeCommand.c_str(), nullptr, 0, nullptr);
	}

	void playWheelAudio(const wchar_t* relativePath, const wchar_t* alias)
	{
		if (relativePath == nullptr || alias == nullptr) {
			return;
		}

		stopWheelAudio(alias);

		const std::wstring audioPath = resolveAudioPath(relativePath);
		if (GetFileAttributesW(audioPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
			return;
		}

		std::wstring openCommand = L"open \"";
		openCommand += audioPath;
		openCommand += L"\" type mpegvideo alias ";
		openCommand += alias;

		if (mciSendStringW(openCommand.c_str(), nullptr, 0, nullptr) != 0) {
			return;
		}

		std::wstring playCommand = L"play ";
		playCommand += alias;
		playCommand += L" from 0";
		mciSendStringW(playCommand.c_str(), nullptr, 0, nullptr);
	}

	std::mt19937& wheelRng()
	{
		static std::mt19937 rng(static_cast<unsigned int>(GetTickCount64()));
		return rng;
	}

	Gdiplus::Color slicePaletteColor(const int index)
	{
		switch (index % 4)
		{
		case 0:
			return Gdiplus::Color(255, GetRValue(kSliceColorBlue), GetGValue(kSliceColorBlue), GetBValue(kSliceColorBlue));
		case 1:
			return Gdiplus::Color(255, GetRValue(kSliceColorWhite), GetGValue(kSliceColorWhite), GetBValue(kSliceColorWhite));
		case 2:
			return Gdiplus::Color(255, GetRValue(kSliceColorRed), GetGValue(kSliceColorRed), GetBValue(kSliceColorRed));
		default:
			return Gdiplus::Color(255, GetRValue(kSliceColorWhite), GetGValue(kSliceColorWhite), GetBValue(kSliceColorWhite));
		}
	}

	int paletteSequenceIndex(const int sliceIndex)
	{
		int normalizedIndex = sliceIndex % 4;
		if (normalizedIndex < 0) {
			normalizedIndex += 4;
		}

		return normalizedIndex;
	}

	Gdiplus::Color brightenColor(const Gdiplus::Color& color, const BYTE amount)
	{
		const BYTE red = static_cast<BYTE>((std::min)(255, color.GetR() + amount));
		const BYTE green = static_cast<BYTE>((std::min)(255, color.GetG() + amount));
		const BYTE blue = static_cast<BYTE>((std::min)(255, color.GetB() + amount));
		return Gdiplus::Color(color.GetA(), red, green, blue);
	}

	Gdiplus::Color blendColors(const Gdiplus::Color& a, const Gdiplus::Color& b, const float ratio)
	{
		const float clampedRatio = (std::max)(0.0f, (std::min)(1.0f, ratio));
		const float inverseRatio = 1.0f - clampedRatio;
		const BYTE red = static_cast<BYTE>(a.GetR() * inverseRatio + b.GetR() * clampedRatio);
		const BYTE green = static_cast<BYTE>(a.GetG() * inverseRatio + b.GetG() * clampedRatio);
		const BYTE blue = static_cast<BYTE>(a.GetB() * inverseRatio + b.GetB() * clampedRatio);
		return Gdiplus::Color(255, red, green, blue);
	}

	bool isLightColor(const Gdiplus::Color& color)
	{
		const int luminance = color.GetR() * 299 + color.GetG() * 587 + color.GetB() * 114;
		return luminance >= 150000;
	}

	void addRoundedRectPath(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect, const float radius)
	{
		const float clampedRadius = (std::max)(0.0f, (std::min)(radius, (std::min)(rect.Width, rect.Height) / 2.0f));
		const float diameter = clampedRadius * 2.0f;

		path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
		path.AddArc(rect.GetRight() - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
		path.AddArc(rect.GetRight() - diameter, rect.GetBottom() - diameter, diameter, diameter, 0.0f, 90.0f);
		path.AddArc(rect.X, rect.GetBottom() - diameter, diameter, diameter, 90.0f, 90.0f);
		path.CloseFigure();
	}

}

SpinnerWheel::~SpinnerWheel()
{
	releaseGdiResources();
}

void SpinnerWheel::loadConfig(const WheelProfile& settings)
{
	wheelCfg_ = settings;
	refreshSectorLabels();
	invalidateDiscCache();

	if (nativeWindow_ != nullptr)
	{
		loadCenterImage();
		InvalidateRect(nativeWindow_, nullptr, FALSE);
	}
}

void SpinnerWheel::showUpdate(const WheelProfile& settings, const HWND owner)
{
	UNREFERENCED_PARAMETER(owner);

	loadConfig(settings);

	if (id() == nullptr)
	{
		if (spawn(
			540, 90,
			760, 860,
			WS_EX_APPWINDOW,
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
			nullptr,
			L"Wheel"))
		{
			ShowWindow(id(), SW_SHOW);
			finishWindowSetup();
		}
		return;
	}

	ShowWindow(id(), SW_SHOW);
	SetForegroundWindow(id());
}

void SpinnerWheel::buildUI()
{
	allocateGdiResources();
	refreshSectorLabels();
	loadCenterImage();
	invalidateDiscCache();

	spinBtn_ = CreateWindowExW(
		0,
		WC_BUTTON,
		L"Spin",
		WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
		0, 0, 140, 34,
		nativeWindow_,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSpinButtonId)),
		nullptr,
		nullptr);

	chromaBtn_ = CreateWindowExW(
		0,
		WC_BUTTON,
		L"Chroma OFF",
		WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
		0, 0, 124, 34,
		nativeWindow_,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(kChromaButtonId)),
		nullptr,
		nullptr);

	updateButtonText();
}

void SpinnerWheel::allocateGdiResources()
{
	if (gdiToken_ == 0)
	{
		Gdiplus::GdiplusStartupInput startupInput;
		if (Gdiplus::GdiplusStartup(&gdiToken_, &startupInput, nullptr) != Gdiplus::Ok) {
			gdiToken_ = 0;
		}
	}
}

void SpinnerWheel::releaseGdiResources()
{
	if (offscreenBuf_ != nullptr)
	{
		delete offscreenBuf_;
		offscreenBuf_ = nullptr;
	}

	if (discBmp_ != nullptr)
	{
		delete discBmp_;
		discBmp_ = nullptr;
	}

	if (hubBmp_ != nullptr)
	{
		delete hubBmp_;
		hubBmp_ = nullptr;
	}

	if (hubImg_ != nullptr)
	{
		delete hubImg_;
		hubImg_ = nullptr;
	}

	if (gdiToken_ != 0)
	{
		Gdiplus::GdiplusShutdown(gdiToken_);
		gdiToken_ = 0;
	}
}

std::wstring SpinnerWheel::trimWhitespace(const std::wstring& text)
{
	const size_t first = text.find_first_not_of(L" \t\r\n");
	if (first == std::wstring::npos) {
		return L"";
	}

	const size_t last = text.find_last_not_of(L" \t\r\n");
	return text.substr(first, last - first + 1);
}

void SpinnerWheel::refreshSectorLabels()
{
	sectorLabels.clear();

	std::wstringstream stream(wheelCfg_.entriesText);
	std::wstring line;
	while (std::getline(stream, line))
	{
		const std::wstring trimmed = trimWhitespace(line);
		if (!trimmed.empty()) {
			sectorLabels.push_back(trimmed);
		}
	}

	if (winnerIdx_ >= static_cast<int>(sectorLabels.size())) {
		winnerIdx_ = -1;
	}

	invalidateDiscCache();
}

void SpinnerWheel::loadCenterImage()
{
	if (hubImg_ != nullptr)
	{
		delete hubImg_;
		hubImg_ = nullptr;
	}

	invalidateCenterCache();

	if (gdiToken_ == 0 || wheelCfg_.centerImg.empty()) {
		return;
	}

	Gdiplus::Image* image = Gdiplus::Image::FromFile(wheelCfg_.centerImg.c_str(), FALSE);
	if (image == nullptr || image->GetLastStatus() != Gdiplus::Ok)
	{
		delete image;
		return;
	}

	hubImg_ = image;
}

void SpinnerWheel::invalidateCenterCache()
{
	if (hubBmp_ != nullptr)
	{
		delete hubBmp_;
		hubBmp_ = nullptr;
	}

	cachedHubDiameter_ = 0;
}

double SpinnerWheel::normalizeAngle(double degrees)
{
	degrees = fmod(degrees, 360.0);
	if (degrees < 0.0) {
		degrees += 360.0;
	}

	return degrees;
}

int SpinnerWheel::determineWinningSector() const
{
	if (sectorLabels.empty()) {
		return -1;
	}

	const double sweep = 360.0 / static_cast<double>(sectorLabels.size());
	const double pointerRelative = normalizeAngle(360.0 - normalizeAngle(angle_) + (sweep / 2.0));
	return static_cast<int>(pointerRelative / sweep) % static_cast<int>(sectorLabels.size());
}

void SpinnerWheel::invalidateDiscCache()
{
	discDirty_ = true;
	cachedDiscDiameter_ = 0;
	lastHighlightedIdx_ = INT_MIN;
	cachedEntryCount_ = -1;
}

void SpinnerWheel::renderCenterImage(const int diameter)
{
	if (hubImg_ == nullptr)
	{
		invalidateCenterCache();
		return;
	}

	const int clampedDiameter = (std::max)(diameter, 1);
	if (hubBmp_ != nullptr && cachedHubDiameter_ == clampedDiameter) {
		return;
	}

	delete hubBmp_;
	hubBmp_ = nullptr;

	const Gdiplus::REAL sourceWidth = static_cast<Gdiplus::REAL>(hubImg_->GetWidth());
	const Gdiplus::REAL sourceHeight = static_cast<Gdiplus::REAL>(hubImg_->GetHeight());
	if (sourceWidth <= 0.0f || sourceHeight <= 0.0f) {
		return;
	}

	hubBmp_ = new Gdiplus::Bitmap(clampedDiameter, clampedDiameter, PixelFormat32bppPARGB);
	Gdiplus::Graphics graphics(hubBmp_);
	graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
	graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
	graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

	Gdiplus::GraphicsPath clipPath;
	clipPath.AddEllipse(0.0f, 0.0f, static_cast<Gdiplus::REAL>(clampedDiameter), static_cast<Gdiplus::REAL>(clampedDiameter));
	graphics.SetClip(&clipPath);

	const Gdiplus::REAL scale = (std::max)(
		static_cast<Gdiplus::REAL>(clampedDiameter) / sourceWidth,
		static_cast<Gdiplus::REAL>(clampedDiameter) / sourceHeight);
	const Gdiplus::REAL drawWidth = sourceWidth * scale;
	const Gdiplus::REAL drawHeight = sourceHeight * scale;
	const Gdiplus::REAL drawX = (static_cast<Gdiplus::REAL>(clampedDiameter) - drawWidth) / 2.0f;
	const Gdiplus::REAL drawY = (static_cast<Gdiplus::REAL>(clampedDiameter) - drawHeight) / 2.0f;

	graphics.DrawImage(
		hubImg_,
		Gdiplus::RectF(drawX, drawY, drawWidth, drawHeight),
		0.0f,
		0.0f,
		sourceWidth,
		sourceHeight,
		Gdiplus::UnitPixel);
	graphics.ResetClip();

	cachedHubDiameter_ = clampedDiameter;
}

void SpinnerWheel::renderDiscBitmap(const int diameter, const int selectedIndex)
{
	(void)selectedIndex;

	if (sectorLabels.empty()) {
		return;
	}

	const int clampedDiameter = (std::max)(diameter, 320);
	if (!discDirty_ &&
		discBmp_ != nullptr &&
		cachedDiscDiameter_ == clampedDiameter &&
		cachedEntryCount_ == static_cast<int>(sectorLabels.size()))
	{
		return;
	}

	delete discBmp_;
	discBmp_ = nullptr;

	const int wheelMargin = 8;
	const int bitmapSize = clampedDiameter + wheelMargin * 2;
	discBmp_ = new Gdiplus::Bitmap(bitmapSize, bitmapSize, PixelFormat32bppPARGB);
	Gdiplus::Graphics graphics(discBmp_);
	graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
	graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
	graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

	const float radius = static_cast<float>(clampedDiameter) / 2.0f;
	const float diameterF = radius * 2.0f;
	const float wheelLeft = static_cast<float>(wheelMargin);
	const float wheelTop = static_cast<float>(wheelMargin);
	const float centerX = wheelLeft + radius;
	const float centerY = wheelTop + radius;
	const float sliceSweep = 360.0f / static_cast<float>(sectorLabels.size());

	for (int index = 0; index < static_cast<int>(sectorLabels.size()); ++index)
	{
		const float startAngle = -90.0f - (sliceSweep / 2.0f) + sliceSweep * index;
		const Gdiplus::Color baseSliceColor = slicePaletteColor(paletteSequenceIndex(index));
		Gdiplus::SolidBrush sliceBrush(baseSliceColor);
		Gdiplus::Pen slicePen(Gdiplus::Color(255, 15, 15, 15), 2.0f);
		graphics.FillPie(&sliceBrush, wheelLeft, wheelTop, diameterF, diameterF, startAngle, sliceSweep);
		graphics.DrawPie(&slicePen, wheelLeft, wheelTop, diameterF, diameterF, startAngle, sliceSweep);
	}

	Gdiplus::Pen outerPen(Gdiplus::Color(255, 5, 5, 5), 6.0f);
	graphics.DrawEllipse(&outerPen, wheelLeft, wheelTop, diameterF, diameterF);

	const float textBandStart = radius * 0.27f + 10.0f;
	const float textBandEnd = radius - 10.0f;
	const float textWidth = (std::max)(58.0f, textBandEnd - textBandStart);
	const float fontSize = 20.0f;

	Gdiplus::FontFamily family(L"Segoe UI");
	Gdiplus::Font textFont(&family, fontSize, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, GetRValue(kWheelText), GetGValue(kWheelText), GetBValue(kWheelText)));
	Gdiplus::StringFormat textFormat;
	textFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
	textFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
	textFormat.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
	textFormat.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

	for (int index = 0; index < static_cast<int>(sectorLabels.size()); ++index)
	{
		const float startAngle = -90.0f - (sliceSweep / 2.0f) + sliceSweep * index;
		const float midAngle = startAngle + (sliceSweep / 2.0f);
		const bool flip = midAngle > 90.0f && midAngle < 270.0f;

		const Gdiplus::GraphicsState state = graphics.Save();
		graphics.TranslateTransform(centerX, centerY);
		graphics.RotateTransform(midAngle);
		if (flip) {
			graphics.RotateTransform(180.0f);
		}

		const float textX = flip ? -textBandEnd : textBandStart;
		const Gdiplus::RectF textRect(textX, -fontSize * 0.82f, textWidth, fontSize * 1.7f);
		graphics.DrawString(
			sectorLabels[index].c_str(),
			-1,
			&textFont,
			textRect,
			&textFormat,
			&textBrush);

		graphics.Restore(state);
	}

	discDirty_ = false;
	cachedDiscDiameter_ = clampedDiameter;
	lastHighlightedIdx_ = 0;
	cachedEntryCount_ = static_cast<int>(sectorLabels.size());
}

void SpinnerWheel::allocateOffscreenBuffer(const int width, const int height)
{
	const int clampedWidth = (std::max)(width, 1);
	const int clampedHeight = (std::max)(height, 1);
	if (offscreenBuf_ != nullptr &&
		cachedBufferW_ == clampedWidth &&
		cachedBufferH_ == clampedHeight)
	{
		return;
	}

	delete offscreenBuf_;
	offscreenBuf_ = new Gdiplus::Bitmap(clampedWidth, clampedHeight, PixelFormat32bppPARGB);
	cachedBufferW_ = clampedWidth;
	cachedBufferH_ = clampedHeight;
}

void SpinnerWheel::drawStyledButton(const DRAWITEMSTRUCT* drawItem) const
{
	if (drawItem == nullptr) {
		return;
	}

	const bool isPressed = (drawItem->itemState & ODS_SELECTED) != 0;
	const bool isFocused = (drawItem->itemState & ODS_FOCUS) != 0;
	const bool isSpinButton = drawItem->CtlID == kSpinButtonId;
	const bool isChromaButton = drawItem->CtlID == kChromaButtonId;

	COLORREF fillColor = RGB(18, 27, 40);
	COLORREF borderColor = RGB(71, 114, 153);
	COLORREF textColor = RGB(244, 249, 255);

	if (isSpinButton && spinning_)
	{
		fillColor = RGB(102, 24, 38);
		borderColor = RGB(255, 93, 118);
		textColor = RGB(255, 245, 248);
	}
	else if (isSpinButton)
	{
		fillColor = RGB(21, 77, 121);
		borderColor = RGB(88, 202, 255);
	}
	else if (isChromaButton && chromaOn_)
	{
		fillColor = RGB(8, 54, 24);
		borderColor = RGB(0, 255, 76);
		textColor = RGB(218, 255, 226);
	}

	if (isPressed) {
		fillColor = adjustColor(fillColor, -18);
	}

	const COLORREF backgroundColor = chromaOn_ ? kChromaKeyGreen : kWheelCard;
	const HBRUSH backgroundBrush = CreateSolidBrush(backgroundColor);
	FillRect(drawItem->hDC, &drawItem->rcItem, backgroundBrush);
	DeleteObject(backgroundBrush);

	RECT textRect = drawItem->rcItem;
	InflateRect(&textRect, -4, -4);
	if (isPressed) {
		OffsetRect(&textRect, 1, 1);
	}

	Gdiplus::Graphics graphics(drawItem->hDC);
	graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

	const float left = static_cast<float>(drawItem->rcItem.left) + 2.0f;
	const float top = static_cast<float>(drawItem->rcItem.top) + 2.0f;
	const int rectWidth = static_cast<int>(drawItem->rcItem.right - drawItem->rcItem.left);
	const int rectHeight = static_cast<int>(drawItem->rcItem.bottom - drawItem->rcItem.top);
	const float width = static_cast<float>((std::max)(1, rectWidth - 4));
	const float height = static_cast<float>((std::max)(1, rectHeight - 4));
	Gdiplus::RectF buttonRect(left, top, width, height);
	Gdiplus::GraphicsPath buttonPath;
	addRoundedRectPath(buttonPath, buttonRect, 7.0f);

	Gdiplus::SolidBrush fillBrush(Gdiplus::Color(255, GetRValue(fillColor), GetGValue(fillColor), GetBValue(fillColor)));
	Gdiplus::Pen borderPen(Gdiplus::Color(255, GetRValue(borderColor), GetGValue(borderColor), GetBValue(borderColor)), 1.6f);
	graphics.FillPath(&fillBrush, &buttonPath);
	graphics.DrawPath(&borderPen, &buttonPath);

	wchar_t buttonText[64] = {};
	GetWindowTextW(drawItem->hwndItem, buttonText, 64);

	SetBkMode(drawItem->hDC, TRANSPARENT);
	SetTextColor(drawItem->hDC, textColor);
	DrawTextW(drawItem->hDC, buttonText, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

	if (isFocused)
	{
		RECT focusRect = drawItem->rcItem;
		InflateRect(&focusRect, -5, -5);
		const HPEN focusPen = CreatePen(PS_SOLID, 1, adjustColor(borderColor, 35));
		const HGDIOBJ oldPen = SelectObject(drawItem->hDC, focusPen);
		const HGDIOBJ oldBrush = SelectObject(drawItem->hDC, GetStockObject(NULL_BRUSH));
		RoundRect(drawItem->hDC, focusRect.left, focusRect.top, focusRect.right, focusRect.bottom, 8, 8);
		SelectObject(drawItem->hDC, oldBrush);
		SelectObject(drawItem->hDC, oldPen);
		DeleteObject(focusPen);
	}
}

void SpinnerWheel::updateButtonText() const
{
	if (spinBtn_ != nullptr) {
		SetWindowTextW(spinBtn_, spinning_ ? L"Stop" : L"Spin");
		RedrawWindow(spinBtn_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
	}

	if (chromaBtn_ != nullptr) {
		SetWindowTextW(chromaBtn_, chromaOn_ ? L"Chroma ON" : L"Chroma OFF");
		RedrawWindow(chromaBtn_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
	}
}

void SpinnerWheel::startSpinAnimation()
{
	if (sectorLabels.size() < 2 || spinning_) {
		return;
	}

	std::uniform_real_distribution<double> velocityDistribution(900.0, 1280.0);
	std::uniform_real_distribution<double> nudgeDistribution(0.0, 40.0);

	spinning_ = true;
	winnerIdx_ = -1;
	invalidateDiscCache();
	stopWheelAudio(kEndSoundAlias);
	playWheelAudio(kSpinSoundRelativePath, kSpinSoundAlias);
	angle_ = normalizeAngle(angle_ + nudgeDistribution(wheelRng()));
	momentum_ = velocityDistribution(wheelRng());
	prevTimestamp_ = GetTickCount64();
	SetTimer(nativeWindow_, kSpinTimerId, kSpinTimerIntervalMs, nullptr);
	updateButtonText();
	InvalidateRect(nativeWindow_, nullptr, FALSE);
}

void SpinnerWheel::stopSpinAnimation()
{
	if (!spinning_) {
		return;
	}

	spinning_ = false;
	momentum_ = 0.0;
	KillTimer(nativeWindow_, kSpinTimerId);
	winnerIdx_ = determineWinningSector();
	stopWheelAudio(kSpinSoundAlias);
	playWheelAudio(kEndSoundRelativePath, kEndSoundAlias);
	invalidateDiscCache();
	updateButtonText();
	InvalidateRect(nativeWindow_, nullptr, FALSE);
}

void SpinnerWheel::tickSpinAnimation()
{
	if (!spinning_) {
		return;
	}

	const ULONGLONG now = GetTickCount64();
	const double deltaSeconds = (std::max)(0.001, static_cast<double>(now - prevTimestamp_) / 1000.0);
	prevTimestamp_ = now;

	angle_ = normalizeAngle(angle_ + momentum_ * deltaSeconds);
	momentum_ = (std::max)(0.0, momentum_ - 260.0 * deltaSeconds);

	if (momentum_ <= 22.0)
	{
		spinning_ = false;
		KillTimer(nativeWindow_, kSpinTimerId);
		winnerIdx_ = determineWinningSector();
		stopWheelAudio(kSpinSoundAlias);
		playWheelAudio(kEndSoundRelativePath, kEndSoundAlias);
		invalidateDiscCache();
		updateButtonText();
	}

	InvalidateRect(nativeWindow_, nullptr, FALSE);
}

void SpinnerWheel::performPaint()
{
	PAINTSTRUCT ps;
	const HDC hdc = BeginPaint(nativeWindow_, &ps);

	RECT clientRect = {};
	GetClientRect(nativeWindow_, &clientRect);

	allocateOffscreenBuffer(clientRect.right, clientRect.bottom);
	if (offscreenBuf_ == nullptr)
	{
		EndPaint(nativeWindow_, &ps);
		return;
	}

	Gdiplus::Graphics bufferGraphics(offscreenBuf_);
	drawCompleteScene(bufferGraphics, clientRect);

	Gdiplus::Graphics graphics(hdc);
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
	graphics.DrawImage(offscreenBuf_, 0, 0);

	EndPaint(nativeWindow_, &ps);
}

void SpinnerWheel::drawCompleteScene(Gdiplus::Graphics& graphics, const RECT& clientRect)
{
	graphics.SetSmoothingMode(spinning_ ? Gdiplus::SmoothingModeHighSpeed : Gdiplus::SmoothingModeHighQuality);
	graphics.SetInterpolationMode(spinning_ ? Gdiplus::InterpolationModeLowQuality : Gdiplus::InterpolationModeHighQualityBicubic);
	graphics.SetPixelOffsetMode(spinning_ ? Gdiplus::PixelOffsetModeNone : Gdiplus::PixelOffsetModeHighQuality);
	graphics.SetCompositingQuality(spinning_ ? Gdiplus::CompositingQualityHighSpeed : Gdiplus::CompositingQualityHighQuality);
	const COLORREF sceneBackground = chromaOn_ ? kChromaKeyGreen : kWheelBackground;
	graphics.Clear(Gdiplus::Color(255, GetRValue(sceneBackground), GetGValue(sceneBackground), GetBValue(sceneBackground)));

	if (!chromaOn_)
	{
		Gdiplus::SolidBrush cardBrush(Gdiplus::Color(255, GetRValue(kWheelCard), GetGValue(kWheelCard), GetBValue(kWheelCard)));
		Gdiplus::Pen accentPen(Gdiplus::Color(255, GetRValue(kWheelAccent), GetGValue(kWheelAccent), GetBValue(kWheelAccent)), 2.0f);
		graphics.DrawRectangle(&accentPen, 10, 10, clientRect.right - 20, clientRect.bottom - 20);
		graphics.FillRectangle(&cardBrush, 16, 16, clientRect.right - 32, clientRect.bottom - 32);
	}

	const int bottomReserved = 90;
	const float centerX = clientRect.right / 2.0f;
	const float centerY = (clientRect.bottom - bottomReserved) / 2.0f + 12.0f;
	const int wheelDiameterLimit = (std::min)(
		static_cast<int>(clientRect.right) - 80,
		static_cast<int>(clientRect.bottom) - bottomReserved - 80);
	const int wheelDiameter = (std::max)(320, wheelDiameterLimit);
	const float radius = static_cast<float>(wheelDiameter) / 2.0f;
	const float diameter = radius * 2.0f;
	const int selectedIndex = spinning_ ? -1 : winnerIdx_;

	if (sectorLabels.empty())
	{
		Gdiplus::FontFamily family(L"Segoe UI");
		Gdiplus::Font font(&family, 24.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
		Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, GetRValue(kWheelTextMuted), GetGValue(kWheelTextMuted), GetBValue(kWheelTextMuted)));
		Gdiplus::StringFormat format;
		format.SetAlignment(Gdiplus::StringAlignmentCenter);
		format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
		Gdiplus::RectF rect(0.0f, 0.0f, static_cast<float>(clientRect.right), static_cast<float>(clientRect.bottom - bottomReserved));
		graphics.DrawString(L"Add at least two sections in Settings to use the wheel.", -1, &font, rect, &format, &textBrush);
		return;
	}

	renderDiscBitmap(static_cast<int>(diameter), selectedIndex);
	if (discBmp_ != nullptr)
	{
		const Gdiplus::GraphicsState state = graphics.Save();
		graphics.TranslateTransform(centerX, centerY);
		graphics.RotateTransform(static_cast<Gdiplus::REAL>(angle_));
		const float bitmapX = -static_cast<float>(discBmp_->GetWidth()) / 2.0f;
		const float bitmapY = -static_cast<float>(discBmp_->GetHeight()) / 2.0f;
		graphics.DrawImage(discBmp_, bitmapX, bitmapY);
		graphics.Restore(state);
	}

	Gdiplus::FontFamily family(L"Segoe UI");
	const float hubOuterRadius = radius * 0.27f;
	const float hubInnerRadius = hubOuterRadius - 10.0f;
	const float imageRadius = hubInnerRadius - 6.0f;
	const float sliceSweep = 360.0f / static_cast<float>(sectorLabels.size());
	const float fontSize = 20.0f;
	if (!spinning_ && selectedIndex >= 0 && selectedIndex < static_cast<int>(sectorLabels.size()))
	{
		const float selectedStartAngle = static_cast<float>(angle_) - 90.0f - (sliceSweep / 2.0f) + sliceSweep * selectedIndex;
		const float wheelLeft = centerX - radius;
		const float wheelTop = centerY - radius;

		Gdiplus::SolidBrush selectedSliceShadowBrush(Gdiplus::Color(64, 0, 0, 0));
		Gdiplus::Pen selectedSliceShadowPen(Gdiplus::Color(148, 0, 0, 0), 5.0f);
		Gdiplus::Pen selectedSliceSoftPen(Gdiplus::Color(72, 0, 0, 0), 10.0f);
		graphics.FillPie(&selectedSliceShadowBrush, wheelLeft, wheelTop, diameter, diameter, selectedStartAngle, sliceSweep);
		graphics.DrawPie(&selectedSliceSoftPen, wheelLeft, wheelTop, diameter, diameter, selectedStartAngle, sliceSweep);
		graphics.DrawPie(&selectedSliceShadowPen, wheelLeft, wheelTop, diameter, diameter, selectedStartAngle, sliceSweep);
	}

	graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
	graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

	Gdiplus::SolidBrush hubShadowBrush(Gdiplus::Color(76, 0, 0, 0));
	Gdiplus::SolidBrush hubSoftShadowBrush(Gdiplus::Color(36, 0, 0, 0));
	graphics.FillEllipse(&hubSoftShadowBrush, centerX - hubOuterRadius - 10.0f, centerY - hubOuterRadius - 10.0f, (hubOuterRadius + 10.0f) * 2.0f, (hubOuterRadius + 10.0f) * 2.0f);
	graphics.FillEllipse(&hubShadowBrush, centerX - hubOuterRadius - 5.0f, centerY - hubOuterRadius - 5.0f, (hubOuterRadius + 5.0f) * 2.0f, (hubOuterRadius + 5.0f) * 2.0f);

	Gdiplus::SolidBrush hubOuterBrush(Gdiplus::Color(255, 33, 37, 47));
	Gdiplus::SolidBrush hubRingBrush(Gdiplus::Color(255, 243, 246, 251));
	Gdiplus::Pen hubOuterPen(Gdiplus::Color(220, 12, 12, 12), 5.0f);
	Gdiplus::Pen hubInnerPen(Gdiplus::Color(220, 75, 86, 103), 2.0f);
	graphics.FillEllipse(&hubOuterBrush, centerX - hubOuterRadius, centerY - hubOuterRadius, hubOuterRadius * 2.0f, hubOuterRadius * 2.0f);
	graphics.FillEllipse(&hubRingBrush, centerX - hubInnerRadius, centerY - hubInnerRadius, hubInnerRadius * 2.0f, hubInnerRadius * 2.0f);
	graphics.DrawEllipse(&hubOuterPen, centerX - hubOuterRadius, centerY - hubOuterRadius, hubOuterRadius * 2.0f, hubOuterRadius * 2.0f);
	graphics.DrawEllipse(&hubInnerPen, centerX - hubInnerRadius, centerY - hubInnerRadius, hubInnerRadius * 2.0f, hubInnerRadius * 2.0f);

	const int centerImageDiameter = (std::max)(1, static_cast<int>(std::round(imageRadius * 2.0f)));
	const float centerImageLeft = centerX - static_cast<float>(centerImageDiameter) / 2.0f;
	const float centerImageTop = centerY - static_cast<float>(centerImageDiameter) / 2.0f;
	if (hubImg_ != nullptr)
	{
		renderCenterImage(centerImageDiameter);
		if (hubBmp_ != nullptr) {
			graphics.DrawImage(hubBmp_, centerImageLeft, centerImageTop);
		}
	}
	else
	{
		Gdiplus::SolidBrush placeholderBrush(Gdiplus::Color(255, 238, 78, 89));
		graphics.FillEllipse(&placeholderBrush, centerX - imageRadius, centerY - imageRadius, imageRadius * 2.0f, imageRadius * 2.0f);
	}

	Gdiplus::Pen imageRingPen(Gdiplus::Color(255, 255, 255, 255), 3.0f);
	graphics.DrawEllipse(&imageRingPen, centerX - imageRadius, centerY - imageRadius, imageRadius * 2.0f, imageRadius * 2.0f);

	if (hubImg_ == nullptr)
	{
		Gdiplus::Font centerFont(&family, imageRadius * 0.40f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
		Gdiplus::SolidBrush centerTextBrush(Gdiplus::Color(255, 255, 255, 255));
		Gdiplus::StringFormat centerFormat;
		centerFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
		centerFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
		Gdiplus::RectF centerTextRect(centerX - imageRadius, centerY - imageRadius, imageRadius * 2.0f, imageRadius * 2.0f);
		graphics.DrawString(L"IMG", -1, &centerFont, centerTextRect, &centerFormat, &centerTextBrush);
	}

	Gdiplus::PointF arrowPoints[3] = {
		Gdiplus::PointF(centerX, centerY - hubOuterRadius - 46.0f),
		Gdiplus::PointF(centerX - 11.0f, centerY - hubOuterRadius - 18.0f),
		Gdiplus::PointF(centerX + 11.0f, centerY - hubOuterRadius - 18.0f)
	};
	Gdiplus::SolidBrush arrowBrush(Gdiplus::Color(255, 0, 0, 0));
	Gdiplus::Pen arrowPen(Gdiplus::Color(255, 0, 0, 0), 2.0f);
	graphics.FillPolygon(&arrowBrush, arrowPoints, 3);
	graphics.DrawPolygon(&arrowPen, arrowPoints, 3);
	graphics.DrawLine(&arrowPen, centerX, centerY - hubOuterRadius - 18.0f, centerX, centerY - hubOuterRadius + 8.0f);

	Gdiplus::Font infoFont(&family, 18.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	Gdiplus::SolidBrush infoBrush(Gdiplus::Color(255, GetRValue(kWheelTextMuted), GetGValue(kWheelTextMuted), GetBValue(kWheelTextMuted)));
	Gdiplus::StringFormat infoFormat;
	infoFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
	infoFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);

	std::wstring footerText = spinning_ ? L"Press Stop to stop" : L"Press Spin to start";

	Gdiplus::RectF footerRect(24.0f, static_cast<float>(clientRect.bottom - 82), static_cast<float>(clientRect.right - 48), 28.0f);
	graphics.DrawString(footerText.c_str(), -1, &infoFont, footerRect, &infoFormat, &infoBrush);
}

LRESULT SpinnerWheel::processMessage(UINT windowMessage, WPARAM wideParameter, LPARAM longParameter)
{
	switch (windowMessage)
	{
	case WM_CREATE:
		buildUI();
		return 0;
	case WM_DESTROY:
		KillTimer(nativeWindow_, kSpinTimerId);
		stopWheelAudio(kSpinSoundAlias);
		stopWheelAudio(kEndSoundAlias);
		releaseGdiResources();
		nativeWindow_ = nullptr;
		return 0;
	case WM_SIZE:
		invalidateDiscCache();
		if (spinBtn_ != nullptr)
		{
			const int clientWidth = LOWORD(longParameter);
			const int clientHeight = HIWORD(longParameter);
			SetWindowPos(spinBtn_, nullptr, (clientWidth - 140) / 2, clientHeight - 44, 140, 32, SWP_NOZORDER);
		}
		if (chromaBtn_ != nullptr)
		{
			const int clientWidth = LOWORD(longParameter);
			SetWindowPos(chromaBtn_, nullptr, clientWidth - 150, 26, 124, 34, SWP_NOZORDER);
		}
		InvalidateRect(nativeWindow_, nullptr, FALSE);
		return 0;
	case WM_COMMAND:
		if (LOWORD(wideParameter) == kSpinButtonId) {
			if (spinning_) {
				stopSpinAnimation();
			}
			else {
				startSpinAnimation();
			}
		}
		else if (LOWORD(wideParameter) == kChromaButtonId) {
			chromaOn_ = !chromaOn_;
			updateButtonText();
			InvalidateRect(nativeWindow_, nullptr, FALSE);
		}
		return 0;
	case WM_DRAWITEM:
		if (wideParameter == kSpinButtonId || wideParameter == kChromaButtonId)
		{
			drawStyledButton(reinterpret_cast<const DRAWITEMSTRUCT*>(longParameter));
			return TRUE;
		}
		break;
	case WM_TIMER:
		if (wideParameter == kSpinTimerId) {
			tickSpinAnimation();
			return 0;
		}
		break;
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
		performPaint();
		return 0;
	default:
		break;
	}

	return DefWindowProc(id(), windowMessage, wideParameter, longParameter);
}

#include "OverlayWindow.h"
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <exception>
#include <sstream>
#include "AppConfig.h"
#include "KeyRouter.h"
#include "BrushFactory.h"
#include "JsonPersistence.h"
#include "NotifyBubble.h"
#include "EntryPoint.h"
#include "AppResources.h"

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

namespace
{
	constexpr int kMinWindowWidth = 285;
	constexpr int kMinWindowHeight = 40;
	constexpr int kImageWindowWidth = 88;
	constexpr int kImageWindowHeight = 168;
	constexpr int kImageMinWindowWidth = 56;
	constexpr int kImageMinWindowHeight = 96;
	constexpr float kTimerFontSize = 25.0f;
	constexpr float kStreakFontSize = 10.5f;
	constexpr float kPi = 3.14159265358979323846f;

	bool pathExists(const std::wstring& path)
	{
		return !path.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	bool isAbsolutePath(const std::wstring& path)
	{
		return path.size() > 2 && path[1] == L':' ||
			(path.size() > 1 && path[0] == L'\\' && path[1] == L'\\');
	}

	std::wstring getModuleDirectory()
	{
		wchar_t modulePath[MAX_PATH] = {};
		const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
		std::wstring directory(modulePath, length);
		const size_t slash = directory.find_last_of(L"\\/");
		if (slash != std::wstring::npos) {
			directory.resize(slash);
		}

		return directory;
	}

	std::wstring joinPath(const std::wstring& directory, const std::wstring& fileName)
	{
		if (directory.empty()) {
			return fileName;
		}

		if (directory.back() == L'\\' || directory.back() == L'/') {
			return directory + fileName;
		}

		return directory + L"\\" + fileName;
	}

	std::wstring resolveImagePath(const std::wstring& path)
	{
		if (pathExists(path) || isAbsolutePath(path)) {
			return path;
		}

		const std::wstring moduleCandidate = joinPath(getModuleDirectory(), path);
		if (pathExists(moduleCandidate)) {
			return moduleCandidate;
		}

		wchar_t currentDirectory[MAX_PATH] = {};
		const DWORD length = GetCurrentDirectoryW(MAX_PATH, currentDirectory);
		if (length > 0 && length < MAX_PATH)
		{
			const std::wstring currentCandidate = joinPath(std::wstring(currentDirectory, length), path);
			if (pathExists(currentCandidate)) {
				return currentCandidate;
			}
		}

		return path;
	}

	D2D1_POINT_2F pointOnCircle(const D2D1_POINT_2F& center, const float radius, const float degrees)
	{
		const float radians = degrees * (kPi / 180.0f);
		return D2D1::Point2F(center.x + (std::cos)(radians) * radius, center.y + (std::sin)(radians) * radius);
	}

	bool canRunTrack(const TimeTracker& track)
	{
		return track.targetMs() > 0;
	}

	void cycleTrackWithReset(TimeTracker& track)
	{
		if (!canRunTrack(track)) {
			return;
		}

		if (track.fetchState() == ClockState::Running) {
			track.pauseCount();
		}
		else if (track.elapsedMs() <= 0) {
			track.clearElapsed();
			track.startCount();
		}
		else if (track.elapsedMs() == track.targetMs()) {
			track.startCount();
		}
		else {
			track.clearElapsed();
		}
	}

	void toggleTrackRun(TimeTracker& track)
	{
		if (!canRunTrack(track)) {
			return;
		}

		if (track.fetchState() == ClockState::Running) {
			track.pauseCount();
		}
		else {
			if (track.elapsedMs() <= 0) {
				track.clearElapsed();
			}
			track.startCount();
		}
	}
}

OverlayWindow::OverlayWindow()
{
	activeClock_ = &primaryClock;
}

OverlayWindow::~OverlayWindow()
{
	reclaimResources();
}

HRESULT OverlayWindow::primeRenderer()
{
	if (d2dFactory_ == nullptr)
	{
		if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory_)))
			return E_FAIL;
	}

	if (dwFactory_ == nullptr)
	{
		if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(dwFactory_), reinterpret_cast<IUnknown**>(&dwFactory_))))
			return E_FAIL;
	}

	if (surface_ == nullptr)
	{
		if (FAILED(constructSurface()))
			return E_FAIL;
	}

	if (clockFmt_ == nullptr && FAILED(forgeClockFmt()))
		return E_FAIL;

	if (streakFmt_ == nullptr && FAILED(forgeStreakFmt()))
		return E_FAIL;

	return S_OK;
}

HRESULT OverlayWindow::forgeClockFmt()
{
	static constexpr WCHAR fontName[] = L"Anago2";
	HRESULT hr = dwFactory_->CreateTextFormat(
		fontName, nullptr,
		DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_EXTRA_EXPANDED, kTimerFontSize, L"", &clockFmt_
	);
	if (FAILED(hr)) return hr;

	clockFmt_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	clockFmt_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
	clockFmt_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
	return S_OK;
}

HRESULT OverlayWindow::forgeStreakFmt()
{
	static constexpr WCHAR streakFontName[] = L"Segoe UI";
	HRESULT hr = dwFactory_->CreateTextFormat(
		streakFontName, nullptr,
		DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL, kStreakFontSize, L"", &streakFmt_
	);
	if (FAILED(hr)) return hr;

	streakFmt_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	streakFmt_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
	streakFmt_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
	return S_OK;
}

HRESULT OverlayWindow::constructSurface()
{
	d2dFactory_->ReloadSystemMetrics();

	RECT rc;
	GetClientRect(nativeWindow_, &rc);

	const D2D1_SIZE_U sz = D2D1::SizeU(rc.right, rc.bottom);
	const D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
		96.0f, 96.0f,
		D2D1_RENDER_TARGET_USAGE_NONE,
		D2D1_FEATURE_LEVEL_DEFAULT
	);

	HRESULT hr = d2dFactory_->CreateHwndRenderTarget(
		props, D2D1::HwndRenderTargetProperties(nativeWindow_, sz), &surface_
	);
	if (FAILED(hr)) return hr;

	const D2D1_COLOR_F cTimer1 = decodeBrush(paletteBrushes_[appSettings_.tintSelection.timerOneColor]);
	const D2D1_COLOR_F cTimer2 = decodeBrush(paletteBrushes_[appSettings_.tintSelection.timerTwoColor]);
	const D2D1_COLOR_F cUrg = decodeBrush(paletteBrushes_[appSettings_.tintSelection.urgentColor]);
	backdropTint_ = decodeBrush(paletteBrushes_[appSettings_.tintSelection.backdropColor]);

	hr = surface_->CreateSolidColorBrush(cTimer1, &inkSlot1_);
	if (FAILED(hr)) return hr;
	hr = surface_->CreateSolidColorBrush(cTimer2, &inkSlot2_);
	if (FAILED(hr)) return hr;
	hr = surface_->CreateSolidColorBrush(cUrg, &inkAlert_);
	if (FAILED(hr)) return hr;
	hr = surface_->CreateSolidColorBrush(D2D1::ColorF(0.78f, 0.9f, 1.0f, 1.0f), &inkStreak_);
	if (FAILED(hr)) return hr;
	hr = surface_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.0f, 0.0f, 0.48f), &inkSweep_);
	if (FAILED(hr)) return hr;
	hr = surface_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.6f), &inkSeam_);
	if (FAILED(hr)) return hr;
	hr = surface_->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.36f), &inkFrame_);
	if (FAILED(hr)) return hr;
	hr = surface_->CreateSolidColorBrush(D2D1::ColorF(0.02f, 0.02f, 0.02f, 1.0f), &inkShadow_);
	if (FAILED(hr)) return hr;

	const D2D1_GRADIENT_STOP borderStops[] = {
		{ 0.0f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.78f) },
		{ 0.55f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.28f) },
		{ 1.0f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f) }
	};
	hr = surface_->CreateGradientStopCollection(borderStops, _countof(borderStops), &frameStops_);
	if (FAILED(hr)) return hr;

	pullBackdrop();
	pullSlotBitmaps(appSettings_);
	return S_OK;
}

HRESULT OverlayWindow::pullBackdrop()
{
	if (surface_ == nullptr) {
		return E_FAIL;
	}

	if (backdropImg_ != nullptr) {
		return S_OK;
	}

	HRESULT hr = S_OK;

	if (wicCore_ == nullptr)
	{
		hr = CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&wicCore_)
		);
	}

	HRSRC hResource = nullptr;
	HGLOBAL hLoadedResource = nullptr;
	void* pResourceData = nullptr;
	DWORD resourceSize = 0;
	IWICStream* pStream = nullptr;
	IWICBitmapDecoder* pDecoder = nullptr;
	IWICBitmapFrameDecode* pFrame = nullptr;
	IWICFormatConverter* pConverter = nullptr;

	if (SUCCEEDED(hr))
	{
		hResource = FindResourceW(hInstance_, MAKEINTRESOURCEW(IDB_TIMER_BACKGROUND), RT_RCDATA);
		if (hResource == nullptr) {
			hr = HRESULT_FROM_WIN32(ERROR_RESOURCE_NAME_NOT_FOUND);
		}
	}

	if (SUCCEEDED(hr))
	{
		hLoadedResource = LoadResource(hInstance_, hResource);
		if (hLoadedResource == nullptr) {
			hr = HRESULT_FROM_WIN32(GetLastError());
		}
	}

	if (SUCCEEDED(hr))
	{
		pResourceData = LockResource(hLoadedResource);
		resourceSize = SizeofResource(hInstance_, hResource);
		if (pResourceData == nullptr || resourceSize == 0) {
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr)) {
		hr = wicCore_->CreateStream(&pStream);
	}
	if (SUCCEEDED(hr)) {
		hr = pStream->InitializeFromMemory(static_cast<BYTE*>(pResourceData), resourceSize);
	}
	if (SUCCEEDED(hr)) {
		hr = wicCore_->CreateDecoderFromStream(pStream, nullptr, WICDecodeMetadataCacheOnLoad, &pDecoder);
	}
	if (SUCCEEDED(hr)) {
		hr = pDecoder->GetFrame(0, &pFrame);
	}
	if (SUCCEEDED(hr)) {
		hr = wicCore_->CreateFormatConverter(&pConverter);
	}
	if (SUCCEEDED(hr))
	{
		hr = pConverter->Initialize(
			pFrame,
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0f,
			WICBitmapPaletteTypeMedianCut
		);
	}
	if (SUCCEEDED(hr)) {
		hr = surface_->CreateBitmapFromWicBitmap(pConverter, nullptr, &backdropImg_);
	}

	SAFE_RELEASE_OBJ(&pStream);
	SAFE_RELEASE_OBJ(&pDecoder);
	SAFE_RELEASE_OBJ(&pFrame);
	SAFE_RELEASE_OBJ(&pConverter);

	return hr;
}

HRESULT OverlayWindow::pullBitmap(const std::wstring& path, ID2D1Bitmap** bitmap)
{
	if (bitmap == nullptr) {
		return E_POINTER;
	}

	*bitmap = nullptr;
	if (surface_ == nullptr || path.empty()) {
		return E_FAIL;
	}

	HRESULT hr = S_OK;
	if (wicCore_ == nullptr)
	{
		hr = CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&wicCore_)
		);
	}

	IWICBitmapDecoder* pDecoder = nullptr;
	IWICBitmapFrameDecode* pFrame = nullptr;
	IWICFormatConverter* pConverter = nullptr;
	const std::wstring resolvedPath = resolveImagePath(path);

	if (SUCCEEDED(hr)) {
		hr = wicCore_->CreateDecoderFromFilename(resolvedPath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
	}
	if (SUCCEEDED(hr)) {
		hr = pDecoder->GetFrame(0, &pFrame);
	}
	if (SUCCEEDED(hr)) {
		hr = wicCore_->CreateFormatConverter(&pConverter);
	}
	if (SUCCEEDED(hr))
	{
		hr = pConverter->Initialize(
			pFrame,
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0f,
			WICBitmapPaletteTypeMedianCut
		);
	}
	if (SUCCEEDED(hr)) {
		hr = surface_->CreateBitmapFromWicBitmap(pConverter, nullptr, bitmap);
	}

	SAFE_RELEASE_OBJ(&pDecoder);
	SAFE_RELEASE_OBJ(&pFrame);
	SAFE_RELEASE_OBJ(&pConverter);

	return hr;
}

HRESULT OverlayWindow::pullSlotBitmaps(const AppConfig& settings)
{
	SAFE_RELEASE_OBJ(&slot1Bmp_);
	SAFE_RELEASE_OBJ(&slot2Bmp_);

	const HRESULT hrPrimaryIcon = pullBitmap(settings.timerOneImgPath, &slot1Bmp_);
	const HRESULT hrSecondaryIcon = pullBitmap(settings.timerTwoImgPath, &slot2Bmp_);
	if (FAILED(hrPrimaryIcon) && FAILED(hrSecondaryIcon)) {
		return hrPrimaryIcon;
	}

	return S_OK;
}

HRESULT OverlayWindow::resizeTypeface(const float fontSize)
{
	SAFE_RELEASE_OBJ(&clockFmt_);

	static constexpr WCHAR fontName[] = L"Anago2";
	HRESULT hr = dwFactory_->CreateTextFormat(
		fontName, nullptr,
		DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_EXTRA_EXPANDED, fontSize, L"", &clockFmt_
	);

	if (FAILED(hr)) return hr;

	clockFmt_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	clockFmt_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
	clockFmt_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
	return S_OK;
}

float OverlayWindow::measureTypeface() const
{
	const float baseRatio = kTimerFontSize / static_cast<float>(kMinWindowHeight);
	return span_[1] * baseRatio;
}

void OverlayWindow::reclaimResources()
{
	SAFE_RELEASE_OBJ(&d2dFactory_);
	SAFE_RELEASE_OBJ(&surface_);
	SAFE_RELEASE_OBJ(&inkSlot1_);
	SAFE_RELEASE_OBJ(&inkSlot2_);
	SAFE_RELEASE_OBJ(&inkAlert_);
	SAFE_RELEASE_OBJ(&inkStreak_);
	SAFE_RELEASE_OBJ(&inkSweep_);
	SAFE_RELEASE_OBJ(&inkSeam_);
	SAFE_RELEASE_OBJ(&inkFrame_);
	SAFE_RELEASE_OBJ(&inkShadow_);
	SAFE_RELEASE_OBJ(&frameStops_);
	SAFE_RELEASE_OBJ(&backdropImg_);
	SAFE_RELEASE_OBJ(&slot1Bmp_);
	SAFE_RELEASE_OBJ(&slot2Bmp_);
	SAFE_RELEASE_OBJ(&dwFactory_);
	SAFE_RELEASE_OBJ(&clockFmt_);
	SAFE_RELEASE_OBJ(&streakFmt_);
	SAFE_RELEASE_OBJ(&wicCore_);
}

HRESULT OverlayWindow::flushSurface() const
{
	const D2D1_SIZE_U newSize = D2D1::SizeU(span_[0], span_[1]);
	return surface_->Resize(newSize);
}

void OverlayWindow::paintBackdrop() const
{
	if (surface_ == nullptr || backdropImg_ == nullptr) {
		return;
	}

	const D2D1_SIZE_F bitmapSize = backdropImg_->GetSize();
	if (bitmapSize.width <= 0.0f || bitmapSize.height <= 0.0f) {
		return;
	}

	const float targetWidth = static_cast<float>(span_[0]);
	const float targetHeight = static_cast<float>(span_[1]);
	const float scale = (std::max)(targetWidth / bitmapSize.width, targetHeight / bitmapSize.height);
	const float sourceWidth = targetWidth / scale;
	const float sourceHeight = targetHeight / scale;
	const float sourceX = (bitmapSize.width - sourceWidth) / 2.0f;
	const float sourceY = (bitmapSize.height - sourceHeight) / 2.0f;

	surface_->DrawBitmap(
		backdropImg_,
		D2D1::RectF(0.0f, 0.0f, targetWidth, targetHeight),
		1.0f,
		D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
		D2D1::RectF(sourceX, sourceY, sourceX + sourceWidth, sourceY + sourceHeight)
	);
}

void OverlayWindow::paintFrame() const
{
	if (surface_ == nullptr || inkFrame_ == nullptr || frameStops_ == nullptr) return;

	const float targetWidth = static_cast<float>(span_[0]);
	const float targetHeight = static_cast<float>(span_[1]);
	const float smokeDepth = (std::min)(16.0f, (std::min)(targetWidth, targetHeight) * 0.35f);

	if (smokeDepth < 4.0f) return;

	const auto fillLinearSmoke = [this](const D2D1_RECT_F& rect, const D2D1_POINT_2F& start, const D2D1_POINT_2F& end)
	{
		ID2D1LinearGradientBrush* pBrush = nullptr;
		if (SUCCEEDED(surface_->CreateLinearGradientBrush(
			D2D1::LinearGradientBrushProperties(start, end),
			frameStops_,
			&pBrush)))
		{
			surface_->FillRectangle(rect, pBrush);
			pBrush->Release();
		}
	};

	fillLinearSmoke(
		D2D1::RectF(0.0f, 0.0f, targetWidth, smokeDepth),
		D2D1::Point2F(0.0f, 0.0f),
		D2D1::Point2F(0.0f, smokeDepth));
	fillLinearSmoke(
		D2D1::RectF(0.0f, targetHeight - smokeDepth, targetWidth, targetHeight),
		D2D1::Point2F(0.0f, targetHeight),
		D2D1::Point2F(0.0f, targetHeight - smokeDepth));
	fillLinearSmoke(
		D2D1::RectF(0.0f, 0.0f, smokeDepth, targetHeight),
		D2D1::Point2F(0.0f, 0.0f),
		D2D1::Point2F(smokeDepth, 0.0f));
	fillLinearSmoke(
		D2D1::RectF(targetWidth - smokeDepth, 0.0f, targetWidth, targetHeight),
		D2D1::Point2F(targetWidth, 0.0f),
		D2D1::Point2F(targetWidth - smokeDepth, 0.0f));

	surface_->DrawRectangle(
		D2D1::RectF(0.5f, 0.5f, targetWidth - 0.5f, targetHeight - 0.5f),
		inkFrame_,
		1.0f
	);
}

EdgeZone OverlayWindow::probeEdge(const LPARAM longParam, const RECT frame) const
{
	const int px = GET_X_LPARAM(longParam);
	const int py = GET_Y_LPARAM(longParam);
	const int spanW = frame.right - frame.left;
	const int spanH = frame.bottom - frame.top;

	const bool nearLeft = (px < gripMargin);
	const bool nearRight = (px > spanW - gripMargin);
	const bool nearTop = (py < gripMargin);
	const bool nearBottom = (py > spanH - gripMargin);

	const bool horizontalEdge = nearLeft || nearRight;
	const bool verticalEdge = nearTop || nearBottom;

	if (!horizontalEdge && !verticalEdge)
		return EdgeZone::ZoneNeutral;

	if (horizontalEdge && verticalEdge)
	{
		if (nearLeft && nearTop) return EdgeZone::ZoneCornerNW;
		if (nearRight && nearTop) return EdgeZone::ZoneCornerNE;
		if (nearLeft && nearBottom) return EdgeZone::ZoneCornerSW;
		return EdgeZone::ZoneCornerSE;
	}

	if (verticalEdge)
		return nearTop ? EdgeZone::ZoneEdgeN : EdgeZone::ZoneEdgeS;

	return nearLeft ? EdgeZone::ZoneEdgeW : EdgeZone::ZoneEdgeE;
}

void OverlayWindow::composeFrame()
{
	if (FAILED(primeRenderer()) || surface_ == nullptr) {
		return;
	}

	PAINTSTRUCT ps;
	BeginPaint(nativeWindow_, &ps);
	surface_->BeginDraw();

	const float fw = static_cast<float>(span_[0]);
	const float fh = static_cast<float>(span_[1]);

	fillBackdrop();
	layClocks(fw, fh);
	layDecor(fw, fh);

	const HRESULT completionCode = surface_->EndDraw();
	const bool surfaceLost = (completionCode == D2DERR_RECREATE_TARGET);
	const bool anyFailure = FAILED(completionCode);
	if (surfaceLost || anyFailure) {
		reclaimResources();
	}

	EndPaint(nativeWindow_, &ps);
}

void OverlayWindow::fillBackdrop()
{
	if (appSettings_.flagSeeThrough) {
		surface_->Clear(D2D1::ColorF(0, 0, 0));
		return;
	}

	switch (appSettings_.flagBgStyle)
	{
	case 1:
		surface_->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f));
		break;
	case 2:
		surface_->Clear(D2D1::ColorF(1.0f, 1.0f, 1.0f));
		break;
	default:
		surface_->Clear(backdropTint_);
		paintBackdrop();
		paintFrame();
		break;
	}
}

void OverlayWindow::layClocks(const float w, const float h)
{
	const bool imgMode = appSettings_.flagTimerImg;

	if (imgMode)
	{
		if (slot1Bmp_ == nullptr || slot2Bmp_ == nullptr) {
			pullSlotBitmaps(appSettings_);
		}
		stampIcon(primaryClock, D2D1::RectF(0, 0, w, h / 2.0f), slot1Bmp_);
		stampIcon(secondaryClock, D2D1::RectF(0, h / 2.0f, w, h), slot2Bmp_);
		return;
	}

	if (dwFactory_ == nullptr || clockFmt_ == nullptr) return;

	auto resolveBrush = [this](const TimeTracker& t, const TimeTracker* active) -> ID2D1SolidColorBrush*
	{
		if (nearingBoundary(t)) return inkAlert_;
		return (active == &primaryClock) ? inkSlot1_ : inkSlot2_;
	};

	ID2D1SolidColorBrush* shadowBrush = appSettings_.flagSeeThrough ? inkShadow_ : nullptr;
	const float shadowDx = 2.0f;
	const float shadowDy = 2.0f;

	primaryClock.paintTimer(surface_, clockFmt_,
		D2D1::RectF(0, 0, w / 2.0f, h), resolveBrush(primaryClock, &primaryClock),
		shadowBrush, shadowDx, shadowDy);
	secondaryClock.paintTimer(surface_, clockFmt_,
		D2D1::RectF(w / 2.0f, 0, w, h), resolveBrush(secondaryClock, &secondaryClock),
		shadowBrush, shadowDx, shadowDy);

	paintSeam(w, h);
}

void OverlayWindow::layDecor(const float w, const float h)
{
	const bool imgMode = appSettings_.flagTimerImg;
	const bool streakOn = streakShowing();

	if (!imgMode && streakOn && streakFmt_ != nullptr && inkStreak_ != nullptr && h >= 32.0f)
	{
		const std::wstring sW = L"Wins: " + streakLabel(0);
		const std::wstring sL = L"Loss: " + streakLabel(1);
		const float lnH = 11.5f;
		const float blk = lnH * 2.0f;
		const float tw = (std::min)(86.0f, w * 0.36f);
		const float mx = w / 2.0f;
		const float tp = (std::max)(0.0f, (h - blk) / 2.0f);

		surface_->DrawTextW(sW.c_str(), static_cast<UINT32>(sW.length()), streakFmt_,
			D2D1::RectF(mx - tw / 2.0f, tp, mx + tw / 2.0f, tp + lnH), inkStreak_);
		surface_->DrawTextW(sL.c_str(), static_cast<UINT32>(sL.length()), streakFmt_,
			D2D1::RectF(mx - tw / 2.0f, tp + lnH, mx + tw / 2.0f, tp + blk), inkStreak_);
	}
}

void OverlayWindow::paintSeam(const float w, const float h)
{
	if (w <= 20.0f || h <= 10.0f || inkSeam_ == nullptr) return;

	const float centerX = w / 2.0f;
	surface_->DrawLine(
		D2D1::Point2F(centerX, 4.0f),
		D2D1::Point2F(centerX, h - 4.0f),
		inkSeam_, 1.0f);
}

void OverlayWindow::trackPointer(const LPARAM secondParam) const
{
	RECT frame;
	GetWindowRect(nativeWindow_, &frame);
	const int cx = GET_X_LPARAM(secondParam);
	const int cy = GET_Y_LPARAM(secondParam);

	const EdgeZone zone = probeEdge(secondParam, frame);

	if (zone != EdgeZone::ZoneNeutral)
	{
		LPCTSTR cursor = IDC_ARROW;
		const int z = static_cast<int>(zone);
		if (z == 0 || z == 6 || z == 8)
			cursor = IDC_SIZENWSE;
		else if (z == 1 || z == 7)
			cursor = IDC_SIZENS;
		else if (z == 2 || z == 3 || z == 5)
			cursor = IDC_SIZEWE;
		SetCursor(LoadCursor(nullptr, cursor));
	}

	if (resizeAxis_ == -1)
	{
		if (pointerHeld_ && !resizeEngaged_)
		{
			const int slideX = frame.left + (cx - dragAnchor_[0]);
			const int slideY = frame.top + (cy - dragAnchor_[1]);
			SetWindowPos(nativeWindow_, nullptr, slideX, slideY, span_[0], span_[1], 0);
		}
	}

	if (pointerHeld_ && resizeEngaged_)
	{
		const int dx = cx - dragAnchor_[0];
		const int dy = cy - dragAnchor_[1];

		int adjustedW = span_[0];
		int adjustedH = span_[1];
		int originX = frame.left;
		int originY = frame.top;

		const float aspectRatio = static_cast<float>(span_[0]) / static_cast<float>(span_[1]);

		if (resizeAxis_ == 2)
		{
			const float diag = std::sqrt(static_cast<float>(dx * dx + dy * dy));
			const float sign = (dx + dy >= 0) ? 1.0f : -1.0f;
			const float scale = 1.0f + (sign * diag / std::sqrt(static_cast<float>(span_[0] * span_[0] + span_[1] * span_[1])));

			adjustedW = static_cast<int>(span_[0] * scale);
			adjustedH = static_cast<int>(adjustedW / aspectRatio);
			originX = frame.left + (span_[0] - adjustedW) / 2;
			originY = frame.top + (span_[1] - adjustedH) / 2;
		}
		else
		{
			const bool horizontalPull = (resizeAxis_ == 1);
			const bool verticalPull = (resizeAxis_ == 0);

			if (horizontalPull)
			{
				const bool pullFromLeft = (cx <= gripMargin);
				if (pullFromLeft) { originX += dx; adjustedW -= dx; }
				else { adjustedW += dx; }
				adjustedH = static_cast<int>(adjustedW / aspectRatio);
			}

			if (verticalPull)
			{
				const bool pullFromTop = (cy <= gripMargin);
				if (pullFromTop) { originY += dy; adjustedH -= dy; }
				else { adjustedH += dy; }
				adjustedW = static_cast<int>(adjustedH * aspectRatio);
			}
		}

		const int floorW = appSettings_.flagTimerImg ? kImageMinWindowWidth : kMinWindowWidth;
		const int floorH = appSettings_.flagTimerImg ? kImageMinWindowHeight : kMinWindowHeight;
		adjustedW = (std::max)(floorW, (std::min)(428, adjustedW));
		adjustedH = (std::max)(floorH, (std::min)(60, adjustedH));
		SetWindowPos(nativeWindow_, nullptr, originX, originY, adjustedW, adjustedH, 0);
	}
}

D2D1_COLOR_F OverlayWindow::decodeBrush(const HBRUSH hBrush)
{
	D2D1_COLOR_F extracted = D2D1::ColorF(0, 0, 0);
	if (!hBrush) return extracted;

	BITMAPINFO descriptor = {};
	descriptor.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	descriptor.bmiHeader.biWidth = 1;
	descriptor.bmiHeader.biHeight = -1;
	descriptor.bmiHeader.biPlanes = 1;
	descriptor.bmiHeader.biBitCount = 32;
	descriptor.bmiHeader.biCompression = BI_RGB;

	void* pixelData = nullptr;
	const HDC screenCtx = GetDC(nullptr);
	const HBITMAP sampleBmp = CreateDIBSection(screenCtx, &descriptor, DIB_RGB_COLORS, &pixelData, nullptr, 0);
	if (sampleBmp)
	{
		const HDC memCtx = CreateCompatibleDC(screenCtx);
		const HGDIOBJ prevSel = SelectObject(memCtx, sampleBmp);
		RECT area = { 0, 0, 1, 1 };
		FillRect(memCtx, &area, hBrush);
		const auto* channels = static_cast<const BYTE*>(pixelData);
		const COLORREF sampled = RGB(channels[2], channels[1], channels[0]);
		extracted = D2D1::ColorF(GetRValue(sampled) / 255.0f, GetGValue(sampled) / 255.0f, GetBValue(sampled) / 255.0f);
		SelectObject(memCtx, prevSel);
		DeleteDC(memCtx);
		DeleteObject(sampleBmp);
	}
	ReleaseDC(nullptr, screenCtx);
	return extracted;
}

void OverlayWindow::regeneratePalette()
{
	const AppConfig settings = loadConfig();

	if (inkSlot1_ != nullptr) {
		inkSlot1_->SetColor(decodeBrush(paletteBrushes_[settings.tintSelection.timerOneColor]));
	}
	if (inkSlot2_ != nullptr) {
		inkSlot2_->SetColor(decodeBrush(paletteBrushes_[settings.tintSelection.timerTwoColor]));
	}
	if (inkAlert_ != nullptr) {
		inkAlert_->SetColor(decodeBrush(paletteBrushes_[settings.tintSelection.urgentColor]));
	}
	if (inkSweep_ != nullptr) {
		inkSweep_->SetColor(D2D1::ColorF(1.0f, 0.0f, 0.0f, 0.48f));
	}
	if (surface_ != nullptr) {
		pullSlotBitmaps(settings);
	}

	backdropTint_ = decodeBrush(paletteBrushes_[settings.tintSelection.backdropColor]);
}

void OverlayWindow::syncClocksFromCfg()
{
	primaryClock.setCountUp(appSettings_.flagCountUp);
	secondaryClock.setCountUp(appSettings_.flagCountUp);
	primaryClock.setTargetMs(appSettings_.timerOneDuration);
	secondaryClock.setTargetMs(appSettings_.timerTwoDuration);
}

float OverlayWindow::calcProgress(const TimeTracker& track) const
{
	const int duration = track.targetMs();
	if (duration <= 0) {
		return 0.0f;
	}

	const float time = static_cast<float>((std::max)(0, (std::min)(duration, track.elapsedMs())));
	const float progress = track.countingUp()
		? time / static_cast<float>(duration)
		: (static_cast<float>(duration) - time) / static_cast<float>(duration);

	return (std::max)(0.0f, (std::min)(1.0f, progress));
}

void OverlayWindow::stampIcon(const TimeTracker& track, const D2D1_RECT_F& rect, ID2D1Bitmap* bitmap) const
{
	if (surface_ == nullptr) {
		return;
	}

	const float width = rect.right - rect.left;
	const float height = rect.bottom - rect.top;
	const float padding = (std::min)(6.0f, (std::min)(width, height) * 0.08f);
	const float imageSize = (std::max)(0.0f, (std::min)(width, height) - (padding * 2.0f));
	if (imageSize <= 0.0f) {
		return;
	}

	const float left = rect.left + ((width - imageSize) / 2.0f);
	const float top = rect.top + ((height - imageSize) / 2.0f);
	const D2D1_RECT_F imageRect = D2D1::RectF(left, top, left + imageSize, top + imageSize);
	const D2D1_POINT_2F center = D2D1::Point2F((imageRect.left + imageRect.right) / 2.0f, (imageRect.top + imageRect.bottom) / 2.0f);
	const float radius = imageSize / 2.0f;

	if (bitmap != nullptr)
	{
		const D2D1_SIZE_F bitmapSize = bitmap->GetSize();
		const D2D1_RECT_F sourceRect = D2D1::RectF(0.0f, 0.0f, bitmapSize.width, bitmapSize.height);
		surface_->DrawBitmap(bitmap, imageRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, sourceRect);
	}
	else if (inkSlot1_ != nullptr && inkSlot2_ != nullptr) {
		ID2D1SolidColorBrush* brush = (&track == &primaryClock) ? inkSlot1_ : inkSlot2_;
		surface_->DrawEllipse(D2D1::Ellipse(center, radius, radius), brush, 1.2f);
	}

	if (inkSweep_ == nullptr) {
		return;
	}

	const float progress = calcProgress(track);
	if (progress <= 0.0f) {
		return;
	}

	ID2D1Geometry* progressGeometry = nullptr;
	if (progress >= 0.999f)
	{
		ID2D1EllipseGeometry* ellipseGeometry = nullptr;
		if (SUCCEEDED(d2dFactory_->CreateEllipseGeometry(D2D1::Ellipse(center, radius, radius), &ellipseGeometry))) {
			progressGeometry = ellipseGeometry;
		}
	}
	else
	{
		ID2D1PathGeometry* pathGeometry = nullptr;
		ID2D1GeometrySink* sink = nullptr;
		if (SUCCEEDED(d2dFactory_->CreatePathGeometry(&pathGeometry)) &&
			SUCCEEDED(pathGeometry->Open(&sink)))
		{
			const D2D1_POINT_2F start = pointOnCircle(center, radius, -90.0f);
			const D2D1_POINT_2F end = pointOnCircle(center, radius, -90.0f + (progress * 360.0f));
			sink->BeginFigure(center, D2D1_FIGURE_BEGIN_FILLED);
			sink->AddLine(start);
			sink->AddArc(D2D1::ArcSegment(
				end,
				D2D1::SizeF(radius, radius),
				0.0f,
				D2D1_SWEEP_DIRECTION_CLOCKWISE,
				progress > 0.5f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
			sink->EndFigure(D2D1_FIGURE_END_CLOSED);
			if (SUCCEEDED(sink->Close())) {
				progressGeometry = pathGeometry;
				pathGeometry = nullptr;
			}
		}

		SAFE_RELEASE_OBJ(&sink);
		SAFE_RELEASE_OBJ(&pathGeometry);
	}

	if (progressGeometry != nullptr)
	{
		if (progress >= 0.999f) {
			inkSweep_->SetColor(D2D1::ColorF(0.0f, 1.0f, 0.22f, 0.58f));
		}
		else if (track.fetchState() == ClockState::Paused) {
			inkSweep_->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.58f));
		}
		else {
			inkSweep_->SetColor(D2D1::ColorF(1.0f, 0.0f, 0.0f, 0.48f));
		}

		surface_->FillGeometry(progressGeometry, inkSweep_);
		SAFE_RELEASE_OBJ(&progressGeometry);
	}
}

void OverlayWindow::flipImgMode()
{
	if (nativeWindow_ == nullptr || prevImgMode_ == appSettings_.flagTimerImg) {
		return;
	}

	prevImgMode_ = appSettings_.flagTimerImg;

	RECT windowRect = {};
		GetWindowRect(nativeWindow_, &windowRect);
	const int targetWidth = appSettings_.flagTimerImg ? kImageWindowWidth : kMinWindowWidth;
	const int targetHeight = appSettings_.flagTimerImg ? kImageWindowHeight : kMinWindowHeight;
	span_[0] = targetWidth;
	span_[1] = targetHeight;
	SetWindowPos(nativeWindow_, nullptr, windowRect.left, windowRect.top, targetWidth, targetHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	if (surface_ != nullptr) {
		flushSurface();
	}
	if (dwFactory_ != nullptr && clockFmt_ != nullptr) {
		resizeTypeface(measureTypeface());
	}
}

bool OverlayWindow::nearingBoundary(const TimeTracker& track) const
{
	return !track.countingUp() &&
		track.targetMs() > 0 &&
		track.elapsedMs() > 0 &&
		track.elapsedMs() <= 20000;
}

bool OverlayWindow::streakShowing() const
{
	return appSettings_.streakData.activeOverlay == 1 ||
		appSettings_.streakData.activeOverlay == 2;
}

std::wstring OverlayWindow::streakLabel(const int lineIndex) const
{
	if (!streakShowing()) {
		return L"0";
	}

	const int presetIndex = appSettings_.streakData.activeOverlay == 2 ? 1 : 0;
	if (presetIndex < 0 || presetIndex >= static_cast<int>(appSettings_.streakData.presets.size())) {
		return L"0";
	}

	std::wstringstream stream(appSettings_.streakData.presets[presetIndex].entries);
	std::wstring line;
	for (int index = 0; index <= lineIndex; ++index)
	{
		if (!std::getline(stream, line)) {
			return L"0";
		}
	}

	line.erase(std::remove(line.begin(), line.end(), L'\r'), line.end());
	const size_t first = line.find_first_not_of(L" \t\r\n");
	if (first == std::wstring::npos) {
		return L"0";
	}

	const size_t last = line.find_last_not_of(L" \t\r\n");
	return line.substr(first, last - first + 1);
}

void OverlayWindow::flipStreak()
{
	if (nativeWindow_ == nullptr) {
		return;
	}

	RECT windowRect = {};
		GetWindowRect(nativeWindow_, &windowRect);
	const int currentWidth = windowRect.right - windowRect.left;
	const int currentHeight = windowRect.bottom - windowRect.top;

	if (currentHeight > kMinWindowHeight && currentHeight <= 74)
	{
		SetWindowPos(nativeWindow_, nullptr, windowRect.left, windowRect.top, currentWidth, kMinWindowHeight, SWP_NOZORDER);
	}
}

LRESULT OverlayWindow::processMessage(UINT windowMessage, WPARAM wideParameter, LPARAM longParameter)
{
	try
	{
		RECT windowPos;
		GetWindowRect(nativeWindow_, &windowPos);

		switch (windowMessage)
		{
	case WM_CREATE:
	{
		if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory_))) {
			return -1;
		}

		syncClocksFromCfg();
		flipImgMode();
		flipStreak();

		PostMessage(nativeWindow_, WM_APP + 16, 0, 0);
		running = true;
		return 0;
	}
	case WM_APP + 16:
	{
		extern HINSTANCE hInstance_;
		InitCommonControls();
		initTintPalette();
		AddFontResourceEx(L"FontsFree-Net-Anago2.ttf", FR_PRIVATE, nullptr);
		finishWindowSetup();
		primeRenderer();
		resizeTypeface(measureTypeface());
		pullBackdrop();
		pullSlotBitmaps(appSettings_);
		InvalidateRect(nativeWindow_, nullptr, FALSE);
		return 0;
	}
		case WM_DESTROY:
			postClose();
			return 0;
	case WM_LBUTTONDOWN:
	{
		dragAnchor_[0] = GET_X_LPARAM(longParameter);
		dragAnchor_[1] = GET_Y_LPARAM(longParameter);

		const EdgeZone zone = probeEdge(longParameter, windowPos);
		const int z = static_cast<int>(zone);

		resizeEngaged_ = (zone != EdgeZone::ZoneNeutral);
		resizeAxis_ = -1;
		if (z == 0 || z == 2 || z == 4 || z == 6)
			resizeAxis_ = 2;
		else if (z == 3 || z == 7)
			resizeAxis_ = 1;
		else if (z == 1 || z == 5)
			resizeAxis_ = 0;

		pointerHeld_ = true;
		SetCapture(nativeWindow_);
		return 0;
	}
		case WM_LBUTTONUP:
		{
			const int finalW = windowPos.right - windowPos.left;
			const int finalH = windowPos.bottom - windowPos.top;
			const bool resized = (finalW != span_[0] || finalH != span_[1]);

			span_[0] = finalW;
			span_[1] = finalH;
			pointerHeld_ = false;
			resizeEngaged_ = false;
			resizeAxis_ = -1;
			ReleaseCapture();

			if (resized) {
				flushSurface();
				resizeTypeface(measureTypeface());
			}
			return 0;
		}
		case WM_MOUSEMOVE:
			trackPointer(longParameter);
			return 0;
		case WM_SIZE:
			span_[0] = LOWORD(longParameter);
			span_[1] = HIWORD(longParameter);
			if (surface_ != nullptr) {
				flushSurface();
			}
			if (dwFactory_ != nullptr && clockFmt_ != nullptr) {
				resizeTypeface(measureTypeface());
			}
			InvalidateRect(nativeWindow_, nullptr, FALSE);
			return 0;
		case WM_COMMAND:
		{
			switch (LOWORD(wideParameter))
			{
			case CMD_MENU_PREFS:
				if (prefsDialog != nullptr)
				{
					if (prefsDialog->id() == nullptr)
					{
						if (!prefsDialog->spawn(
							500, 200,
							WIN_PREFS_W, WIN_PREFS_H,
							0,
							WS_OVERLAPPED | WS_CAPTION | WS_MINIMIZEBOX,
							nativeWindow_,
							L"Settings - v0.3"))
						{
							return 0;
						}

						ShowWindow(prefsDialog->id(), SW_SHOW);
					}
					else
					{
						SetForegroundWindow(prefsDialog->id());
					}
				}
				return 0;
			case CMD_MENU_EXIT:
				postClose();
				return 0;
			default:
				break;
			}
			return 0;
		}
		case WM_CONTEXTMENU:
		{
			const int mouseX = GET_X_LPARAM(longParameter);
			const int mouseY = GET_Y_LPARAM(longParameter);

			HMENU hMenu = CreatePopupMenu();
			AppendMenuW(hMenu, MF_STRING, CMD_MENU_PREFS, L"Settings");
			AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
			AppendMenuW(hMenu, MF_STRING, CMD_MENU_EXIT, L"Quit");
			const int cmd = TrackPopupMenuEx(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY, mouseX, mouseY, nativeWindow_, nullptr);
			if (cmd > 0) {
				PostMessage(nativeWindow_, WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
			}
			DestroyMenu(hMenu);
			return 0;
		}
		case WM_SETCURSOR:
		{
			POINT pt;
			GetCursorPos(&pt);
			ScreenToClient(nativeWindow_, &pt);
			RECT frame;
			GetWindowRect(nativeWindow_, &frame);
			const LPARAM lp = MAKELPARAM(pt.x, pt.y);
			const EdgeZone zone = probeEdge(lp, frame);
			if (zone != EdgeZone::ZoneNeutral)
			{
				LPCTSTR cursor = IDC_ARROW;
				const int z = static_cast<int>(zone);
				if (z == 0 || z == 4 || z == 6 || z == 8)
					cursor = IDC_SIZENWSE;
				else if (z == 1 || z == 5)
					cursor = IDC_SIZENS;
				else if (z == 2 || z == 3 || z == 7)
					cursor = IDC_SIZEWE;
				SetCursor(LoadCursor(nullptr, cursor));
			}
			else
			{
				SetCursor(LoadCursor(nullptr, IDC_ARROW));
			}
			return TRUE;
		}
		case WM_ERASEBKGND:
			return 1;
		case WM_PAINT:
			composeFrame();
			return 0;
		case WM_TIMER_TICK:
		{
			const int primaryMillisBefore = primaryClock.elapsedMs();
			const int secondaryMillisBefore = secondaryClock.elapsedMs();
			const ClockState primaryPhaseBefore = primaryClock.fetchState();
			const ClockState secondaryPhaseBefore = secondaryClock.fetchState();

			primaryClock.advance();
			secondaryClock.advance();

			if (primaryMillisBefore != primaryClock.elapsedMs() ||
				secondaryMillisBefore != secondaryClock.elapsedMs() ||
				primaryPhaseBefore != primaryClock.fetchState() ||
				secondaryPhaseBefore != secondaryClock.fetchState())
			{
				RedrawWindow(nativeWindow_, nullptr, nullptr, RDW_INVALIDATE);
			}

			return 0;
		}
		case WM_REFRESH_TINTS:
			regeneratePalette();
			syncClocksFromCfg();
			flipImgMode();
			flipStreak();
			RedrawWindow(nativeWindow_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
			return 0;
		case WM_HOTKEY_PRESSED:
			ingestHotkey(static_cast<int>(wideParameter));
			return 0;
		case WM_PAD_EVENT:
			ingestGamepad(static_cast<WORD>(wideParameter));
			return 0;
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

void OverlayWindow::ingestHotkey(const int code)
{
	switch (code)
	{
	case IDX_TMR_ONE:
		activeClock_ = &primaryClock;
		if (appSettings_.flagAutoStart) {
			cycleTrackWithReset(primaryClock);
		}
		break;
	case IDX_TMR_TWO:
		activeClock_ = &secondaryClock;
		if (appSettings_.flagAutoStart) {
			cycleTrackWithReset(secondaryClock);
		}
		break;
	case IDX_BOTH_TIMERS:
		cycleTrackWithReset(primaryClock);
		cycleTrackWithReset(secondaryClock);
		break;
	case IDX_TOXIC_CHAT:
		if (prefsDialog != nullptr && prefsDialog->chatOverlay != nullptr)
		{
			if (prefsDialog->chatOverlay->isVisible()) {
				prefsDialog->chatOverlay->setVisible(false);
			}
			else if (appSettings_.notifyProfile.toxicChatTimed) {
				prefsDialog->chatOverlay->flash(appSettings_.notifyProfile.toxicChatDurationMs);
			}
			else {
				prefsDialog->chatOverlay->setVisible(true);
			}
		}
		break;
	default:
		break;
	}

	RedrawWindow(nativeWindow_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
}

void OverlayWindow::ingestGamepad(const WORD buttons) const
{
	if (prefsDialog != nullptr && prefsDialog->id() != nullptr)
	{
		SendMessage(prefsDialog->id(), WM_PAD_EVENT, buttons, NULL);
		return;
	}

	KeyRouter::route(buttons);
}

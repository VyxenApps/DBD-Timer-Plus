#pragma once
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include "NativeSurface.h"
#include "TimeTracker.h"
#include "PrefsPanel.h"

enum EdgeZone : uint8_t
{
	ZoneCornerNW, ZoneEdgeN, ZoneCornerNE,
	ZoneEdgeE, ZoneCornerSE, ZoneEdgeS,
	ZoneCornerSW, ZoneEdgeW, ZoneNeutral
};

class OverlayWindow : public NativeFrame
{
private:
	ID2D1Factory* d2dFactory_ = nullptr;
	ID2D1HwndRenderTarget* surface_ = nullptr;
	ID2D1SolidColorBrush* inkSlot1_ = nullptr;
	ID2D1SolidColorBrush* inkSlot2_ = nullptr;
	ID2D1SolidColorBrush* inkAlert_ = nullptr;
	ID2D1SolidColorBrush* inkStreak_ = nullptr;
	ID2D1SolidColorBrush* inkSweep_ = nullptr;
	ID2D1SolidColorBrush* inkSeam_ = nullptr;
	ID2D1SolidColorBrush* inkFrame_ = nullptr;
	ID2D1SolidColorBrush* inkShadow_ = nullptr;
	ID2D1GradientStopCollection* frameStops_ = nullptr;
	ID2D1Bitmap* backdropImg_ = nullptr;
	ID2D1Bitmap* slot1Bmp_ = nullptr;
	ID2D1Bitmap* slot2Bmp_ = nullptr;
	D2D1_COLOR_F backdropTint_;

	IDWriteFactory* dwFactory_ = nullptr;
	IDWriteTextFormat* clockFmt_ = nullptr;
	IDWriteTextFormat* streakFmt_ = nullptr;
	IWICImagingFactory* wicCore_ = nullptr;

	TimeTracker* activeClock_ = nullptr;

	BOOL pointerHeld_ = false;
	int dragAnchor_[2] = { 0, 0 };
	bool resizeEngaged_ = false;
	int resizeAxis_ = -1;
	int gripMargin = 8;
	bool prevImgMode_ = false;

	HRESULT primeRenderer();
	HRESULT forgeClockFmt();
	HRESULT forgeStreakFmt();
	HRESULT constructSurface();
	HRESULT pullBackdrop();
	HRESULT pullBitmap(const std::wstring& path, ID2D1Bitmap** out);
	HRESULT pullSlotBitmaps(const AppConfig& cfg);
	HRESULT resizeTypeface(float sz);
	float measureTypeface() const;
	void reclaimResources();
	HRESULT flushSurface() const;
	void paintBackdrop() const;
	void paintFrame() const;
	void paintSeam(float w, float h);
	EdgeZone probeEdge(LPARAM lp, RECT bounds) const;
	void composeFrame();
	void fillBackdrop();
	void layClocks(float w, float h);
	void layDecor(float w, float h);
	void trackPointer(LPARAM lp) const;
	static D2D1_COLOR_F decodeBrush(HBRUSH br);
	void regeneratePalette();
	void syncClocksFromCfg();
	float calcProgress(const TimeTracker& clk) const;
	void stampIcon(const TimeTracker& clk, const D2D1_RECT_F& area, ID2D1Bitmap* bmp) const;
	void flipImgMode();
	bool nearingBoundary(const TimeTracker& clk) const;
	void flipStreak();
	bool streakShowing() const;
	std::wstring streakLabel(int idx) const;

public:
	TimeTracker primaryClock;
	TimeTracker secondaryClock;
	PrefsPanel* prefsDialog = nullptr;

	OverlayWindow();
	OverlayWindow(const OverlayWindow&) = delete;
	OverlayWindow& operator=(const OverlayWindow&) = delete;
	OverlayWindow(OverlayWindow&&) = delete;
	OverlayWindow& operator=(OverlayWindow&&) noexcept = delete;
	~OverlayWindow();

	LPCWSTR identifyClass() const override { return L"TimerOverlayCls"; }
	LRESULT processMessage(UINT msg, WPARAM wp, LPARAM lp) override;
	void ingestHotkey(int keyCode);
	void ingestGamepad(WORD mask) const;
};

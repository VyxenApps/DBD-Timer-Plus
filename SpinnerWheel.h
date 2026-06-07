#pragma once
#include "NativeSurface.h"
#include "AppConfig.h"
#include <climits>
#include <gdiplus.h>
#include <string>
#include <vector>

class SpinnerWheel : public NativeFrame
{
private:
	WheelProfile wheelCfg_ = {};
	std::vector<std::wstring> sectorLabels;
	HWND spinBtn_ = nullptr;
	HWND chromaBtn_ = nullptr;
	ULONG_PTR gdiToken_ = 0;

	Gdiplus::Image* hubImg_ = nullptr;
	Gdiplus::Bitmap* hubBmp_ = nullptr;
	Gdiplus::Bitmap* discBmp_ = nullptr;
	Gdiplus::Bitmap* offscreenBuf_ = nullptr;

	double angle_ = 0.0;
	double momentum_ = 0.0;
	ULONGLONG prevTimestamp_ = 0;
	bool spinning_ = false;
	bool chromaOn_ = false;
	bool discDirty_ = true;
	int winnerIdx_ = -1;

	int cachedBufferW_ = 0;
	int cachedBufferH_ = 0;
	int cachedDiscDiameter_ = 0;
	int cachedHubDiameter_ = 0;
	int lastHighlightedIdx_ = INT_MIN;
	int cachedEntryCount_ = -1;

	void buildUI();
	void allocateGdiResources();
	void releaseGdiResources();
	void refreshSectorLabels();
	void loadCenterImage();
	void invalidateCenterCache();
	void invalidateDiscCache();
	void renderCenterImage(int diameter);
	void renderDiscBitmap(int diameter, int selectedIdx);
	void allocateOffscreenBuffer(int width, int height);
	void performPaint();
	void drawCompleteScene(Gdiplus::Graphics& g, const RECT& rc);
	void drawStyledButton(const DRAWITEMSTRUCT* di) const;
	void updateButtonText() const;
	void startSpinAnimation();
	void stopSpinAnimation();
	void tickSpinAnimation();
	int determineWinningSector() const;
	static double normalizeAngle(double degrees);
	static std::wstring trimWhitespace(const std::wstring& text);

public:
	SpinnerWheel() = default;
	~SpinnerWheel();

	void loadConfig(const WheelProfile& settings);
	void showUpdate(const WheelProfile& settings, HWND ownerWnd);

	LPCWSTR identifyClass() const override { return L"RoulettePopup"; }
	LRESULT processMessage(UINT windowMessage, WPARAM wideParameter, LPARAM longParameter) override;
};

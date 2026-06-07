#pragma once
#include "NativeSurface.h"
#include <gdiplus.h>

class NotifyBubble : public NativeFrame
{
private:
	bool visible_ = false;
	bool autoHideActive_ = false;
	NotifyProfile overlayCfg_ = {};
	ULONG_PTR gdiToken_ = 0;
	Gdiplus::Image* bgImg_ = nullptr;
	std::wstring curImgPath_;

	RECT computeOverlayBounds() const;
	void createid();
	void maskWindow() const;
	void loadBgImage();
	void releaseBgImage();
	void armAutoHide(int durationMillis) const;
	void onAutoHide();

public:
	~NotifyBubble();

	void setVisible(bool show);
	void flash(int durationMillis);
	bool isVisible() const { return visible_; }
	void applyConfig(const NotifyProfile& settings);

	LPCWSTR identifyClass() const override { return L"ToastMsgFrame"; }
	LRESULT processMessage(UINT windowMessage, WPARAM wideParameter, LPARAM longParameter) override;
};

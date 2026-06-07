#pragma once
#include "NativeSurface.h"

class PalettePopup : public NativeFrame
{
private:
	int swatchIdx_ = 0;

	void buildSwatchGrid();
	void paintSwatches() const;
	void handleSwatchClick(int x, int y);
	void applySelectedColor(int colorIndex);
	int getCurrentSelection() const;

public:
	int originCtrlId = 0;
	AppConfig* parentCfg = nullptr;

	LRESULT processMessage(UINT windowMessage, WPARAM wideParameter, LPARAM longParameter) override;
	LPCWSTR identifyClass() const override { return L"ColorSwatchPopup"; }
};

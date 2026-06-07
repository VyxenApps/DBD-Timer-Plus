#pragma once
#include "PalettePopup.h"
#include "AppConfig.h"
#include <gdiplus.h>
#include <initializer_list>
#include <vector>

class SpinnerWheel;
class LoadoutEditor;
class NotifyBubble;

enum class SettingsPage
{
	PageTimers,
	PageWheel,
	PageBuilds,
	PageExtra,
	PageOverlays
};

class PrefsPanel : public NativeFrame
{
	struct WidgetCfg { LPCWSTR cls = nullptr; LPCWSTR txt = nullptr; int x = 0; int y = 0; int w = 0; int h = 0; int id = 0; long sty = 0; };

private:
	AppConfig editedConfig = {};
	SettingsPage currentTab = SettingsPage::PageTimers;
	HWND activeHotkeyControl_ = nullptr;
	HWND hndTabTimers_ = nullptr;
	HWND hndTabWheel_ = nullptr;
	HWND hndTabBuilds_ = nullptr;
	HWND hndTabExtra_ = nullptr;
	HWND hndTabOverlays_ = nullptr;
	HWND hndTimersTitle_ = nullptr;
	HWND hndWheelTitle_ = nullptr;
	HWND hndBuildsTitle_ = nullptr;
	HWND hndTmr1Dur_ = nullptr;
	HWND hndTmr2Dur_ = nullptr;
	HWND hndWhlPresets_ = nullptr;
	HWND hndWhlRename_ = nullptr;
	HWND hndWhlItems_ = nullptr;
	HWND hndWhlScroll_ = nullptr;
	HWND hndWhlImgPath_ = nullptr;
	HWND hndStkPresets_ = nullptr;
	HWND hndStkItems_ = nullptr;
	HWND hndStkScroll_ = nullptr;
	HWND hndStkSurvWins_ = nullptr;
	HWND hndStkSurvLoss_ = nullptr;
	HWND hndStkKlrWins_ = nullptr;
	HWND hndStkKlrLoss_ = nullptr;
	HWND hndOtxKey_ = nullptr;
	HWND hndOtxTimed_ = nullptr;
	HWND hndOtxDur_ = nullptr;
	HWND hndOtxImgPath_ = nullptr;
	HWND hndTmrImgLbl_ = nullptr;
	HWND hndBackdropBtn_ = nullptr;
	HWND hndBackdropSurface_ = nullptr;
	HWND hndBackdropOpts_[3] = {};
	HBRUSH editBrush_ = nullptr;
	HBRUSH renameBrush_ = nullptr;
	ULONG_PTR gdiToken_ = 0;
	HBITMAP bgDib_ = nullptr;
	void* bgDibBits_ = nullptr;
	int bgDibW_ = 0;
	int bgDibH_ = 0;
	std::vector<HWND> timerCtrls_;
	std::vector<HWND> wheelCtrls_;
	std::vector<HWND> buildCtrls_;
	std::vector<HWND> extraCtrls_;
	std::vector<HWND> overlayCtrls_;
	int wheelPresetSel_ = 0;
	int whlRenameIdx_ = -1;
	bool whlScrollDrag_ = false;
	bool stkScrollDrag_ = false;
	bool backdropOpen_ = false;
	bool secondaryPagesVisible_ = false;
	bool mouseTracked_ = false;
	bool cfgApplied_ = false;
	int whlScrollOff_ = 0;
	int stkScrollOff_ = 0;
	int streakPresetSel_ = 0;

	byte gridRows_ = 24;
	byte gridCols_ = 13;

	int rowH_ = WIN_PREFS_H / gridRows_;
	int colW_ = WIN_PREFS_W / gridCols_;

	void layoutPanel();
	void createThemeResources();
	void discardThemeResources();
	void invalidateBgDib();
	void ensureBgDib(int w, int h);
	void applyChromeTheme() const;
	void paintWindow() const;
	void placeLabels();
	void placeButtons();
	void buildTimingPanelText();
	void buildSettingsTopTabs(int clientWidth);
	void buildDurationFields(int xDuration, int widthDuration, int heightDuration, int sizeCheckbox);
	void buildHotkeyRowsForTiming(int xHotkey, int xHotkeyCon, int widthHotkey, int heightHotkey);
	void buildTimingModeToggles(int xCheckbox, int sizeCheckbox);
	void buildTimingColorRows(int xColorButton, int y, int width, int height, int rowGap);
	void buildFooterActions(int clientWidth);
	void rememberTimingControls(std::initializer_list<HWND> controls);
	void placeWheelControls();
	void placeBuildControls();
	void placeExtraControls();
	void placeOverlayControls();
	void trackTabControl(HWND control, SettingsPage tab);
	void setActiveTab(SettingsPage tab);
	void setSecondaryPagesVisible(bool isVisible);
	void updateHeaderTitleVisibility() const;
	void redrawTabStrip() const;
	void updateSecondaryTabHover(POINT point);
	bool isPointInsideSecondaryTabs(POINT point) const;
	std::wstring getControlText(HWND control) const;
	bool applyWheelInputs(bool requireReadyForOpen);
	bool applyOverlayInputs(bool validateDuration);
	void syncWheelPresetFromInputs();
	void loadWheelPresetIntoInputs(int presetIndex);
	void refreshWheelPresetList();
	void selectWheelPreset(int presetIndex);
	void beginWheelPresetRename(int presetIndex = -1);
	void commitWheelPresetRename(bool applyChanges);
	void browseForWheelImage();
	void browseForOverlayImage();
	void syncWheelItemsScrollbar();
	void setWheelItemsTopLine(int topLine);
	void drawWheelItemsScrollbar(HDC hdc) const;
	RECT getWheelItemsScrollThumbRect() const;
	int getWheelItemsVisibleLineCount() const;
	int getWheelItemsMaxTopLine() const;
	void beginWheelItemsScrollDrag(int mouseY);
	void updateWheelItemsScrollDrag(int mouseY);
	void endWheelItemsScrollDrag();
	void syncStreakPresetFromInputs();
	void loadStreakPresetIntoInputs(int presetIndex);
	void refreshStreakPresetList();
	void selectStreakPreset(int presetIndex);
	void syncStreakItemsScrollbar();
	void setStreakItemsTopLine(int topLine);
	void drawStreakItemsScrollbar(HDC hdc) const;
	RECT getStreakItemsScrollThumbRect() const;
	int getStreakItemsVisibleLineCount() const;
	int getStreakItemsMaxTopLine() const;
	void beginStreakItemsScrollDrag(int mouseY);
	void updateStreakItemsScrollDrag(int mouseY);
	void endStreakItemsScrollDrag();
	void updateStreakToggleControls() const;
	void updateOverlayToggleControls() const;
	void setBackdropMenuVisible(bool isOpen);
	bool pointHitsBackdropMenu(POINT point) const;

	HWND spawnWidget(const WidgetCfg& cfg) const;

	static void applyBrandingFont(HWND hControl);
	void onWidgetNotify(WPARAM firstParam, LPARAM secondParam);
	bool applyDurationInputs();
	bool isCheckboxChecked(int controlId) const;
	void drawOwnerDrawControl(const DRAWITEMSTRUCT* drawItem) const;
	void drawThemeButton(const DRAWITEMSTRUCT* drawItem) const;
	void drawThemeCheckbox(const DRAWITEMSTRUCT* drawItem) const;
	void drawThemeColorButton(const DRAWITEMSTRUCT* drawItem) const;
	void drawBackdropLauncher(const DRAWITEMSTRUCT* drawItem) const;
	void drawBackdropPalette(const DRAWITEMSTRUCT* drawItem) const;
	void drawBackdropSwatch(const DRAWITEMSTRUCT* drawItem) const;
	void drawThemePresetComboItem(const DRAWITEMSTRUCT* drawItem) const;
	void paintThemePresetCombo(HWND comboHwnd, HDC hdc = nullptr) const;

	void stashCtrlHotkey(UINT key);
	void stashKeyHotkey(UINT key);
	void restoreHotkeyLabel(HWND hCtrl);

	static bool isCheckboxControlId(int controlId);
	static bool isColorControlId(int controlId);
	static bool isBackdropChoiceId(int controlId);
	static bool isTitleText(const wchar_t* text);
	static bool isMutedText(const wchar_t* text);

	static LRESULT CALLBACK panelCallback(
		HWND idWin,
		UINT uMsg,
		WPARAM firstParam,
		LPARAM secondParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData);

	struct BindEntry { UINT code; const wchar_t* tag; };

	static constexpr BindEntry kbEntries[] = {
		{0x01, L"M1"}, {0x02, L"M2"}, {0x04, L"M3"},
		{0x05, L"M4"}, {0x06, L"M5"},
		{0x12, L"Alt"}, {0x11, L"Ctrl"}, {0x10, L"Shift"},
		{0x14, L"BloqMay"}, {0x09, L"Tab"}, {0x1B, L"Esc"},
		{0xC0, L"~"},
		{0x25, L"Izq"}, {0x27, L"Der"}, {0x26, L"Arr"}, {0x28, L"Aba"},
		{0x0D, L"Enter"}, {0x08, L"Borrar"}, {0x20, L"Space"},
		{0x0C, L"Limpiar"}, {0x13, L"Pausa"},
		{0x21, L"RePag"}, {0x22, L"AvPag"}, {0x23, L"Fin"}, {0x24, L"Inic"},
		{0x29, L"Sel"}, {0x2A, L"Impr"}, {0x2B, L"Ejec"},
		{0x2C, L"Capt"}, {0x2D, L"Ins"}, {0x2E, L"Supr"}, {0x2F, L"Ayuda"},
		{0x30, L"0"}, {0x31, L"1"}, {0x32, L"2"}, {0x33, L"3"},
		{0x34, L"4"}, {0x35, L"5"}, {0x36, L"6"}, {0x37, L"7"},
		{0x38, L"8"}, {0x39, L"9"},
		{0x41, L"A"}, {0x42, L"B"}, {0x43, L"C"}, {0x44, L"D"},
		{0x45, L"E"}, {0x46, L"F"}, {0x47, L"G"}, {0x48, L"H"},
		{0x49, L"I"}, {0x4A, L"J"}, {0x4B, L"K"}, {0x4C, L"L"},
		{0x4D, L"M"}, {0x4E, L"N"}, {0x4F, L"O"}, {0x50, L"P"},
		{0x51, L"Q"}, {0x52, L"R"}, {0x53, L"S"}, {0x54, L"T"},
		{0x55, L"U"}, {0x56, L"V"}, {0x57, L"W"}, {0x58, L"X"},
		{0x59, L"Y"}, {0x5A, L"Z"},
		{0x5B, L"WinI"}, {0x5C, L"WinD"}, {0x5D, L"Menu"},
		{0x5F, L"Susp"},
		{0x60, L"N0"}, {0x61, L"N1"}, {0x62, L"N2"}, {0x63, L"N3"},
		{0x64, L"N4"}, {0x65, L"N5"}, {0x66, L"N6"}, {0x67, L"N7"},
		{0x68, L"N8"}, {0x69, L"N9"},
		{0x6A, L"NMul"}, {0x6B, L"NSum"}, {0x6C, L"NSep"},
		{0x6D, L"NRes"}, {0x6E, L"NDec"}, {0x6F, L"NDiv"},
		{0x70, L"F1"}, {0x71, L"F2"}, {0x72, L"F3"}, {0x73, L"F4"},
		{0x74, L"F5"}, {0x75, L"F6"}, {0x76, L"F7"}, {0x77, L"F8"},
		{0x78, L"F9"}, {0x79, L"F10"}, {0x7A, L"F11"}, {0x7B, L"F12"},
		{0x7C, L"F13"}, {0x7D, L"F14"}, {0x7E, L"F15"}, {0x7F, L"F16"},
		{0x80, L"F17"}, {0x81, L"F18"}, {0x82, L"F19"}, {0x83, L"F20"},
		{0x84, L"F21"}, {0x85, L"F22"}, {0x86, L"F23"}, {0x87, L"F24"},
		{0x90, L"BloqN"}, {0x91, L"BloqD"},
		{0xA6, L"NavA"}, {0xA7, L"NavAd"}, {0xA8, L"NavR"},
		{0xA9, L"NavD"}, {0xAA, L"NavB"}, {0xAB, L"NavF"}, {0xAC, L"NavI"},
		{0xAD, L"Sil"}, {0xAE, L"Vol-"}, {0xAF, L"Vol+"},
		{0xB0, L"Sig"}, {0xB1, L"Ant"}, {0xB2, L"Det"}, {0xB3, L"Play"},
		{0xB4, L"Mail"}, {0xB5, L"Med"}, {0xB6, L"App1"}, {0xB7, L"App2"},
		{0xBA, L";"}, {0xBB, L"="}, {0xBC, L","}, {0xBD, L"-"},
		{0xBE, L"."}, {0xBF, L"/"}, {0xC0, L"`"},
		{0xDB, L"["}, {0xDC, L"\\"}, {0xDD, L"]"}, {0xDE, L"'"},
		{0xDF, L"?"}, {0xF3, L"Attn"}, {0xF4, L"Crsel"},
		{0xF5, L"Exsel"}, {0xF6, L"Ereof"}, {0xFA, L"Play"},
		{0xFB, L"Zoom"}, {0xFC, L"PA1"}, {0xFD, L"Limpiar"},
	};
	static constexpr int kbEntryCount = sizeof(kbEntries) / sizeof(kbEntries[0]);

	static constexpr BindEntry padEntries[] = {
		{CTL_DIR_UP, L"CruArr"}, {CTL_DIR_DOWN, L"CruAba"},
		{CTL_DIR_RIGHT, L"CruDer"}, {CTL_DIR_LEFT, L"CruIzq"},
		{CTL_BTN_MENU, L"Inic"}, {CTL_BTN_VIEW, L"Vista"},
		{CTL_BTN_LSTICK, L"PalI"}, {CTL_BTN_RSTICK, L"PalD"},
		{CTL_BTN_LSHOULDER, L"LB"}, {CTL_BTN_RSHOULDER, L"RB"},
		{CTL_BTN_A, L"BtnA"}, {CTL_BTN_B, L"BtnB"},
		{CTL_BTN_X, L"BtnX"}, {CTL_BTN_Y, L"BtnY"},
		{CTL_TRIG_L, L"GatI"}, {CTL_TRIG_R, L"GatD"},
	};
	static constexpr int padEntryCount = sizeof(padEntries) / sizeof(padEntries[0]);

	static const wchar_t* lookupKb(UINT vk);
	static const wchar_t* lookupPad(UINT code);

public:
	PalettePopup* colorPicker = nullptr;
	SpinnerWheel* pSpinnerWheel = nullptr;
	LoadoutEditor* pSideSurvivorLoadoutEditor = nullptr;
	LoadoutEditor* pSideKillerLoadoutEditor = nullptr;
	NotifyBubble* chatOverlay = nullptr;

	PrefsPanel() = default;
	~PrefsPanel();

	void onColorMsg(LPARAM longParam) const;
	LRESULT processMessage(UINT windowMessage, WPARAM wideParameter, LPARAM longParameter) override;
	LPCWSTR identifyClass() const override { return L"TimerCfgDlg"; }
};

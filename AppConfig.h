#pragma once
#include <array>
#include <string>
#include <vector>
#include <Windows.h>

constexpr byte CMD_ACCEPT = 2001;
constexpr byte CMD_DISMISS = 2002;
constexpr byte CMD_BEGIN = 2003;
constexpr byte CMD_TIMER_ONE = 2004;
constexpr byte CMD_TIMER_TWO = 2005;
constexpr byte CMD_BEGIN_NR = 2006;
constexpr byte CMD_PAGE_TIMERS = 2007;
constexpr byte CMD_PAGE_WHEEL = 2008;
constexpr byte CMD_PAGE_BUILDS = 2009;
constexpr byte CMD_WHEEL_ITEMS = 2010;
constexpr byte CMD_WHEEL_IMAGE = 2011;
constexpr byte CMD_WHEEL_BROWSE = 2012;
constexpr byte CMD_WHEEL_SHOW = 2013;
constexpr byte CMD_WHEEL_PRESET = 2014;
constexpr byte CMD_WHEEL_RENAME = 2015;
constexpr byte CMD_WHEEL_SCROLLBAR = 2016;
constexpr byte CMD_CHK_SEETHROUGH = 2017;
constexpr byte CMD_CHK_PASSTHRU = 2018;
constexpr byte CMD_CHK_AUTO_BEGIN = 2019;
constexpr byte CMD_CHK_COUNTUP = 2020;
constexpr byte CMD_PAD_BIND_BEGIN = 2021;
constexpr byte CMD_PAD_BIND_TMR1 = 2022;
constexpr byte CMD_PAD_BIND_TMR2 = 2023;
constexpr byte CMD_PAD_BIND_BEGIN_NR = 2024;
constexpr byte CMD_DUR_INPUT_TMR1 = 2025;
constexpr byte CMD_DUR_INPUT_TMR2 = 2026;
constexpr byte CMD_MENU_EXIT = 2027;
constexpr byte CMD_MENU_PREFS = 2028;
constexpr byte IDX_TMR_ONE = 2031;
constexpr byte IDX_TMR_TWO = 2032;
constexpr byte IDX_BOTH_TIMERS = 2033;
constexpr byte IDX_TOXIC_CHAT = 2034;
constexpr byte OPT_SEETHROUGH = 2035;
constexpr byte OPT_PASSTHRU = 2036;

constexpr byte CMD_CLR_TIMER = 2037;
constexpr byte CMD_CLR_SEL_TIMER = 2038;
constexpr byte CMD_CLR_URGENT = 2039;
constexpr byte CMD_CLR_BACKDROP = 2040;
constexpr byte CMD_CLR_SAMPLE = 2041;
constexpr byte CMD_BUILD_SURV = 2042;
constexpr byte CMD_BUILD_KLR = 2043;
constexpr byte CMD_BG_STYLE_GEAR = 2044;
constexpr byte CMD_BG_STYLE_CUR = 2045;
constexpr byte CMD_BG_STYLE_BLK = 2046;
constexpr byte CMD_BG_STYLE_WHT = 2047;
constexpr byte CMD_BG_STYLE_PNL = 2048;
constexpr byte CMD_PAGE_EXTRA = 2049;
constexpr byte CMD_CHK_STREAK_SURV = 2050;
constexpr byte CMD_CHK_STREAK_KLR = 2051;
constexpr byte CMD_STREAK_PRESET = 2052;
constexpr byte CMD_STREAK_ITEMS = 2053;
constexpr byte CMD_STREAK_SCROLL = 2054;
constexpr byte CMD_STREAK_SURV_WIN = 2055;
constexpr byte CMD_STREAK_SURV_LOSS = 2056;
constexpr byte CMD_STREAK_KLR_WIN = 2057;
constexpr byte CMD_STREAK_KLR_LOSS = 2058;
constexpr byte CMD_PAGE_OVERLAYS = 2059;
constexpr byte CMD_OVERLAY_TOGGLE = 2060;
constexpr byte CMD_OVERLAY_KEY = 2061;
constexpr byte CMD_CHK_OVERLAY_TIMED = 2062;
constexpr byte CMD_OVERLAY_DUR = 2063;
constexpr byte CMD_OVERLAY_IMG = 2064;
constexpr byte CMD_OVERLAY_BROWSE = 2065;
constexpr byte CMD_CHK_TIMER_IMAGE = 2066;

constexpr USHORT CTL_DIR_UP = 7001;
constexpr USHORT CTL_DIR_DOWN = 7002;
constexpr USHORT CTL_DIR_RIGHT = 7003;
constexpr USHORT CTL_DIR_LEFT = 7004;
constexpr USHORT CTL_BTN_MENU = 7005;
constexpr USHORT CTL_BTN_VIEW = 7006;
constexpr USHORT CTL_BTN_LSTICK = 7007;
constexpr USHORT CTL_BTN_RSTICK = 7008;
constexpr USHORT CTL_BTN_LSHOULDER = 7009;
constexpr USHORT CTL_BTN_RSHOULDER = 7010;
constexpr USHORT CTL_BTN_A = 7011;
constexpr USHORT CTL_BTN_B = 7012;
constexpr USHORT CTL_BTN_X = 7013;
constexpr USHORT CTL_BTN_Y = 7014;
constexpr USHORT CTL_TRIG_L = 7015;
constexpr USHORT CTL_TRIG_R = 7016;

constexpr USHORT XINPUT_START = 0x0400;
constexpr USHORT XINPUT_BACK = 0x0800;
constexpr USHORT XINPUT_L3 = 0x1000;
constexpr USHORT XINPUT_R3 = 0x2000;
constexpr USHORT XINPUT_LB = 0x4000;
constexpr USHORT XINPUT_RB = 0x8000;
constexpr USHORT XINPUT_A = 0x0040;
constexpr USHORT XINPUT_B = 0x0080;
constexpr USHORT XINPUT_X = 0x0100;
constexpr USHORT XINPUT_Y = 0x0200;

constexpr UINT16 DLG_PREFS_W = 400;
constexpr UINT16 DLG_PREFS_H = 880;
constexpr UINT16 DLG_TINT_W = 500;
constexpr UINT16 DLG_TINT_H = 550;
constexpr UINT16 TINT_PALETTE_SZ = 105;
constexpr UINT16 WIN_PREFS_W = 400;
constexpr UINT16 WIN_PREFS_H = 880;

constexpr byte IDB_MOUSE_ICON = 2067;
constexpr byte IDB_PAD_ICON = 2068;

constexpr int WM_REFRESH_TINTS(WM_APP + 12);
constexpr int WM_HOTKEY_PRESSED(WM_APP + 13);
constexpr int WM_PAD_EVENT(WM_APP + 14);
constexpr int WM_TIMER_TICK(WM_APP + 15);

#define CONFIG_FILE "AppPrefs.json"

struct TintPalette
{
	int timerOneColor = 3;
	int timerTwoColor = 5;
	int urgentColor = 8;
	int backdropColor = 20;
};

inline std::wstring defaultWheelLines()
{
	return L"Option 1\r\nOption 2\r\nOption 3\r\nOption 4";
}

struct WheelSlot
{
	std::wstring label = L"Preset 1";
	std::wstring entries = defaultWheelLines();
};

inline WheelSlot makeWheelSlot(const wchar_t* presetLabel)
{
	WheelSlot entry;
	entry.label = presetLabel;
	return entry;
}

struct WheelProfile
{
	int activePreset = 0;
	std::array<WheelSlot, 5> presets = {
		makeWheelSlot(L"Preset 1"),
		makeWheelSlot(L"Preset 2"),
		makeWheelSlot(L"Preset 3"),
		makeWheelSlot(L"Preset 4"),
		makeWheelSlot(L"Preset 5")
	};
	std::wstring entriesText = defaultWheelLines();
	std::wstring centerImg = L"";
};

struct PerkSlot
{
	std::wstring label = L"New Build";
	std::array<std::wstring, 4> perks = {};
};

struct LoadoutBundle
{
	std::vector<PerkSlot> survivor;
	std::vector<PerkSlot> killer;
};

struct StreakSlot
{
	std::wstring label = L"SideSurvivor";
	std::wstring entries = L"0\r\n0";
};

inline StreakSlot makeStreakSlot(const wchar_t* presetLabel)
{
	StreakSlot entry;
	entry.label = presetLabel;
	return entry;
}

struct StreakBundle
{
	int activeOverlay = 0;
	int activePreset = 0;
	std::array<StreakSlot, 2> presets = {
		makeStreakSlot(L"SideSurvivor"),
		makeStreakSlot(L"SideKiller")
	};
};

struct NotifyProfile
{
	bool toxicChatEnabled = false;
	int toxicChatKey = VK_F9;
	bool toxicChatTimed = false;
	int toxicChatDurationMs = 15000;
	std::wstring toxicChatImg = L"";
};

struct AppConfig
{
	int timerTwoDuration = 60000;
	int timerOneDuration = 60000;
	int padTimerTwoKey = CTL_DIR_RIGHT;
	int padTimerOneKey = CTL_DIR_LEFT;
	int padStartNrKey = CTL_BTN_B;
	int padStartKey = CTL_BTN_A;
	int timerTwoKey = 0x05;
	int timerOneKey = 0x06;

	unsigned int flagAutoStart : 1;
	unsigned int flagCountUp : 1;
	unsigned int flagSeeThrough : 1;
	unsigned int flagPassThru : 1;
	unsigned int flagBgStyle : 2;
	unsigned int flagTimerImg : 1;
	unsigned int flagPad : 1;

	AppConfig() { flagAutoStart = 0; flagCountUp = 0; flagSeeThrough = 0; flagPassThru = 0; flagBgStyle = 0; flagTimerImg = 0; flagPad = 0; }

	TintPalette tintSelection;
	StreakBundle streakData;
	WheelProfile wheelData;
	NotifyProfile notifyProfile;
	LoadoutBundle loadoutData;
	std::wstring timerTwoImgPath = L"TimerImages\\iconPerks_DecisiveStrike.png";
	std::wstring timerOneImgPath = L"TimerImages\\protecc.png";
};

extern HINSTANCE hInstance_;
extern HBRUSH paletteBrushes_[TINT_PALETTE_SZ];
extern AppConfig appSettings_;
extern HWND overlayHwnd_;

#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Window.h"
#include "ConfigStore.h"
#include "RenderPipeline.h"

struct TimerSlot
{
	int durationMs = 30000;
	bool countUp = false;
};

struct HotkeySlot
{
	UINT keyboard = 0;
	UINT gamepad = 0;
};

class AppBuilder
{
	std::wstring fontName_ = L"Anago2";
	float fontSize_ = 25.0f;
	std::vector<TimerSlot> timers_;
	std::vector<HotkeySlot> hotkeys_;
	int windowWidth_ = 285;
	int windowHeight_ = 40;
	DWORD windowExStyle_ = WS_EX_TOPMOST | WS_EX_LAYERED;
	DWORD windowStyle_ = WS_POPUP;
	COLORREF layeredKey_ = 1;
	BYTE layeredAlpha_ = 255;
	std::wstring configPath_ = L"AppPrefs.json";
	bool clickThrough_ = false;
	bool transparent_ = false;

public:
	AppBuilder& withFont(const std::wstring& name, float size)
	{
		fontName_ = name;
		fontSize_ = size;
		return *this;
	}

	AppBuilder& withTimer(int durationMs, bool countUp = false)
	{
		timers_.push_back({durationMs, countUp});
		return *this;
	}

	AppBuilder& withHotkey(UINT keyboard, UINT gamepad = 0)
	{
		hotkeys_.push_back({keyboard, gamepad});
		return *this;
	}

	AppBuilder& withWindowSize(int w, int h)
	{
		windowWidth_ = w;
		windowHeight_ = h;
		return *this;
	}

	AppBuilder& withWindowStyle(DWORD exStyle, DWORD style)
	{
		windowExStyle_ = exStyle;
		windowStyle_ = style;
		return *this;
	}

	AppBuilder& withLayered(COLORREF key, BYTE alpha)
	{
		layeredKey_ = key;
		layeredAlpha_ = alpha;
		return *this;
	}

	AppBuilder& withConfig(const std::wstring& path)
	{
		configPath_ = path;
		return *this;
	}

	AppBuilder& withClickThrough(bool enabled)
	{
		clickThrough_ = enabled;
		return *this;
	}

	AppBuilder& withTransparent(bool enabled)
	{
		transparent_ = enabled;
		return *this;
	}

	struct AppSpec
	{
		std::wstring fontName;
		float fontSize;
		std::vector<TimerSlot> timers;
		std::vector<HotkeySlot> hotkeys;
		int windowWidth;
		int windowHeight;
		DWORD windowExStyle;
		DWORD windowStyle;
		COLORREF layeredKey;
		BYTE layeredAlpha;
		std::wstring configPath;
		bool clickThrough;
		bool transparent;
	};

	AppSpec build() const
	{
		return {
			fontName_, fontSize_,
			timers_, hotkeys_,
			windowWidth_, windowHeight_,
			windowExStyle_, windowStyle_,
			layeredKey_, layeredAlpha_,
			configPath_,
			clickThrough_, transparent_
		};
	}
};

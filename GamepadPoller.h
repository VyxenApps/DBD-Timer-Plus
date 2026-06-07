#pragma once

#include "NativeSurface.h"
#include <atomic>
#include <functional>
#include <map>
#include <thread>
#include <Xinput.h>

enum PadBtn : USHORT
{
	PadA = CTL_BTN_A,
	PadB = CTL_BTN_B,
	PadX = CTL_BTN_X,
	PadY = CTL_BTN_Y,
	PadUp = CTL_DIR_UP,
	PadDown = CTL_DIR_DOWN,
	PadRight = CTL_DIR_RIGHT,
	PadLeft = CTL_DIR_LEFT,
	PadMenu = CTL_BTN_MENU,
	PadView = CTL_BTN_VIEW,
	PadLThumb = CTL_BTN_LSTICK,
	PadRThumb = CTL_BTN_RSTICK,
	PadLShoulder = CTL_BTN_LSHOULDER,
	PadRShoulder = CTL_BTN_RSHOULDER,
	PadLTrigger = CTL_TRIG_L,
	PadRTrigger = CTL_TRIG_R
};

class GamepadPoller
{
public:
	std::map<USHORT, USHORT> btnXlat = {
		{XINPUT_GAMEPAD_DPAD_UP, CTL_DIR_UP},
		{XINPUT_GAMEPAD_DPAD_DOWN, CTL_DIR_DOWN},
		{XINPUT_GAMEPAD_DPAD_LEFT, CTL_DIR_LEFT},
		{XINPUT_GAMEPAD_DPAD_RIGHT, CTL_DIR_RIGHT},
		{XINPUT_GAMEPAD_START, CTL_BTN_MENU},
		{XINPUT_START, CTL_BTN_MENU},
		{XINPUT_GAMEPAD_BACK, CTL_BTN_VIEW},
		{XINPUT_BACK, CTL_BTN_VIEW},
		{XINPUT_GAMEPAD_LEFT_THUMB, CTL_BTN_LSTICK},
		{XINPUT_L3, CTL_BTN_LSTICK},
		{XINPUT_GAMEPAD_RIGHT_THUMB, CTL_BTN_RSTICK},
		{XINPUT_R3, CTL_BTN_RSTICK},
		{XINPUT_GAMEPAD_LEFT_SHOULDER, CTL_BTN_LSHOULDER},
		{XINPUT_LB, CTL_BTN_LSHOULDER},
		{XINPUT_GAMEPAD_RIGHT_SHOULDER, CTL_BTN_RSHOULDER},
		{XINPUT_RB, CTL_BTN_RSHOULDER},
		{XINPUT_GAMEPAD_A, CTL_BTN_A},
		{XINPUT_A, CTL_BTN_A},
		{XINPUT_GAMEPAD_B, CTL_BTN_B},
		{XINPUT_B, CTL_BTN_B},
		{XINPUT_GAMEPAD_X, CTL_BTN_X},
		{XINPUT_X, CTL_BTN_X},
		{XINPUT_GAMEPAD_Y, CTL_BTN_Y},
		{XINPUT_Y, CTL_BTN_Y},
	};

private:
	XINPUT_STATE currentState_ = {};
	XINPUT_STATE prevState_ = {};
	std::function<void(WORD)> callback_;
	std::atomic<bool> active_ = false;
	std::thread pollThread_;

public:
	GamepadPoller();

	void setHandler(const std::function<void(WORD)>& callback);
	void beginPoll();
	void endPoll();
	void buildXlatTable();

private:
	void readState();
};

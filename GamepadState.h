#pragma once
#include <Windows.h>
#include <Xinput.h>

class GamepadState
{
	XINPUT_STATE raw_ = {};
	bool connected_ = false;
	int slot_ = 0;

public:
	GamepadState() = default;

	bool poll(int controllerSlot = 0)
	{
		slot_ = controllerSlot;
		connected_ = (XInputGetState(slot_, &raw_) == ERROR_SUCCESS);
		return connected_;
	}

	bool isConnected() const { return connected_; }
	int slot() const { return slot_; }

	WORD rawButtons() const { return raw_.Gamepad.wButtons; }
	BYTE rawLeftTrigger() const { return raw_.Gamepad.bLeftTrigger; }
	BYTE rawRightTrigger() const { return raw_.Gamepad.bRightTrigger; }
	SHORT rawLeftThumbX() const { return raw_.Gamepad.sThumbLX; }
	SHORT rawLeftThumbY() const { return raw_.Gamepad.sThumbLY; }
	SHORT rawRightThumbX() const { return raw_.Gamepad.sThumbRX; }
	SHORT rawRightThumbY() const { return raw_.Gamepad.sThumbRY; }

	bool isPressed(WORD mask) const
	{
		return connected_ && (raw_.Gamepad.wButtons & mask) != 0;
	}

	bool isTriggerActive(bool left) const
	{
		if (!connected_) return false;
		const BYTE val = left ? raw_.Gamepad.bLeftTrigger : raw_.Gamepad.bRightTrigger;
		return val > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
	}

	WORD buttonsChanged(WORD previousButtons) const
	{
		return (raw_.Gamepad.wButtons ^ previousButtons) & raw_.Gamepad.wButtons;
	}
};

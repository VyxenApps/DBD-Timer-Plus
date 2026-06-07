#include "GamepadPoller.h"
#include "GamepadState.h"

GamepadPoller::GamepadPoller()
{
	active_.store(false, std::memory_order_relaxed);
}

void GamepadPoller::setHandler(const std::function<void(WORD)>& callback)
{
	callback_ = callback;
}

void GamepadPoller::readState()
{
	GamepadState pad;
	if (!pad.poll(0))
	{
		prevState_ = {};
		return;
	}

	constexpr BYTE trigThresh = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;

	const bool leftTrigFell = prevState_.Gamepad.bLeftTrigger >= trigThresh
		&& pad.rawLeftTrigger() < trigThresh;
	const bool rightTrigFell = prevState_.Gamepad.bRightTrigger >= trigThresh
		&& pad.rawRightTrigger() < trigThresh;

	const WORD newlyPressed = pad.buttonsChanged(prevState_.Gamepad.wButtons & 0xFFFF);

	if (newlyPressed != 0)
	{
		auto it = btnXlat.find(newlyPressed);
		if (it != btnXlat.end()) {
			callback_(it->second);
		}
	}
	else if (leftTrigFell)
	{
		callback_(PadBtn::PadLTrigger);
	}
	else if (rightTrigFell)
	{
		callback_(PadBtn::PadRTrigger);
	}

	prevState_.Gamepad.wButtons = pad.rawButtons();
	prevState_.Gamepad.bLeftTrigger = pad.rawLeftTrigger();
	prevState_.Gamepad.bRightTrigger = pad.rawRightTrigger();
}

void GamepadPoller::beginPoll()
{
	if (active_.load(std::memory_order_acquire)) return;

	XInputEnable(TRUE);
	active_.store(true, std::memory_order_release);

	pollThread_ = std::thread([this]()
	{
		while (active_.load(std::memory_order_acquire))
		{
			readState();
			Sleep(30);
		}

		XInputEnable(FALSE);
	});
}

void GamepadPoller::endPoll()
{
	active_.store(false, std::memory_order_release);
	if (pollThread_.joinable())
	{
		pollThread_.join();
	}
}

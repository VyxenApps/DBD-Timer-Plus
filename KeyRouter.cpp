#include "AppConfig.h"
#include "KeyRouter.h"

#include <algorithm>

#include "EntryPoint.h"
#include "OverlayWindow.h"

namespace
{
	int findAction(const KeyRouter::BindSlot* slots, int count, int keyCode)
	{
		for (int i = 0; i < count; ++i)
		{
			if (slots[i].key == keyCode) {
				return slots[i].action;
			}
		}
		return -1;
	}
}

std::array<KeyRouter::BindSlot, 16> KeyRouter::slots_;
int KeyRouter::slotCount_ = 0;

void KeyRouter::loadKeys(const AppConfig& settings)
{
	slotCount_ = 0;

	auto addSlot = [&](int key, int action)
	{
		if (slotCount_ < static_cast<int>(slots_.size()) && key != 0)
		{
			slots_[slotCount_] = { key, action };
			++slotCount_;
		}
	};

	addSlot(settings.timerOneKey, IDX_TMR_ONE);
	addSlot(settings.timerTwoKey, IDX_TMR_TWO);
	addSlot(settings.notifyProfile.toxicChatKey, IDX_TOXIC_CHAT);

	addSlot(settings.padTimerOneKey, IDX_TMR_ONE);
	addSlot(settings.padTimerTwoKey, IDX_TMR_TWO);
}

void KeyRouter::loadDirect(int beginKey, int beginNrKey, int tmrOneKey, int tmrTwoKey, int padBeginKey, int padBeginNrKey, int padTmrOneKey, int padTmrTwoKey)
{
	slotCount_ = 0;

	auto addSlot = [&](int key, int action)
	{
		if (slotCount_ < static_cast<int>(slots_.size()) && key != 0)
		{
			slots_[slotCount_] = { key, action };
			++slotCount_;
		}
	};

	addSlot(tmrOneKey, IDX_TMR_ONE);
	addSlot(tmrTwoKey, IDX_TMR_TWO);

	addSlot(padTmrOneKey, IDX_TMR_ONE);
	addSlot(padTmrTwoKey, IDX_TMR_TWO);
}

void KeyRouter::route(const int inputCode)
{
	if (overlayWindow_ == nullptr || overlayWindow_->id() == nullptr)
	{
		return;
	}

	const int action = findAction(slots_.data(), slotCount_, inputCode);
	if (action < 0) {
		return;
	}

	const bool isTimer1 = (action == IDX_TMR_ONE);
	const bool isTimer2 = (action == IDX_TMR_TWO);

	if (isTimer1 || isTimer2)
	{
		bool foundTmr1 = false;
		bool foundTmr2 = false;
		for (int i = 0; i < slotCount_; ++i)
		{
			if (slots_[i].key == inputCode)
			{
				if (slots_[i].action == IDX_TMR_ONE) foundTmr1 = true;
				if (slots_[i].action == IDX_TMR_TWO) foundTmr2 = true;
			}
		}

		if (foundTmr1 && foundTmr2) {
			PostMessage(overlayWindow_->id(), WM_HOTKEY_PRESSED, IDX_BOTH_TIMERS, 0);
		}
		else {
			PostMessage(overlayWindow_->id(), WM_HOTKEY_PRESSED, action, 0);
		}
	}
	else
	{
		PostMessage(overlayWindow_->id(), WM_HOTKEY_PRESSED, action, 0);
	}
}

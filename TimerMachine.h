#pragma once
#include <chrono>
#include <functional>
#include <map>

enum class TimerState : uint8_t
{
	Idle,
	CountingUp,
	CountingDown,
	Paused,
	Finished
};

enum class TimerEvent : uint8_t
{
	Start,
	Pause,
	Resume,
	Reset,
	Tick,
	ReachTarget,
	Clear
};

class TimerMachine
{
	using Clock = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;

	struct Transition
	{
		TimerState destination;
		std::function<void()> action;
	};

	TimerState current_;
	int elapsedMs_ = 0;
	int targetMs_ = 0;
	bool countUp_ = false;
	TimePoint lastTick_;
	std::map<std::pair<TimerState, TimerEvent>, Transition> transitions_;

public:
	TimerMachine()
		: current_(TimerState::Idle)
	{
		addTransition(TimerState::Idle, TimerEvent::Start, {TimerState::CountingUp, nullptr});
		addTransition(TimerState::Idle, TimerEvent::Start, {TimerState::CountingDown, nullptr});
		addTransition(TimerState::CountingUp, TimerEvent::Pause, {TimerState::Paused, nullptr});
		addTransition(TimerState::CountingDown, TimerEvent::Pause, {TimerState::Paused, nullptr});
		addTransition(TimerState::Paused, TimerEvent::Resume, {TimerState::CountingUp, nullptr});
		addTransition(TimerState::Paused, TimerEvent::Resume, {TimerState::CountingDown, nullptr});
		addTransition(TimerState::CountingUp, TimerEvent::Reset, {TimerState::Idle, nullptr});
		addTransition(TimerState::CountingDown, TimerEvent::Reset, {TimerState::Idle, nullptr});
		addTransition(TimerState::Paused, TimerEvent::Reset, {TimerState::Idle, nullptr});
		addTransition(TimerState::CountingDown, TimerEvent::ReachTarget, {TimerState::Finished, nullptr});
		addTransition(TimerState::Finished, TimerEvent::Clear, {TimerState::Idle, nullptr});
	}

	void addTransition(TimerState from, TimerEvent event, Transition transition)
	{
		transitions_[{from, event}] = transition;
	}

	void processEvent(TimerEvent event)
	{
		auto key = std::make_pair(current_, event);
		auto it = transitions_.find(key);
		if (it != transitions_.end())
		{
			if (it->second.action)
				it->second.action();
			current_ = it->second.destination;
		}
	}

	void tick()
	{
		if (current_ == TimerState::Idle)
			return;

		const auto now = Clock::now();
		const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick_).count();
		lastTick_ = now;

		if (current_ == TimerState::CountingUp)
		{
			elapsedMs_ += static_cast<int>(delta);
		}
		else if (current_ == TimerState::CountingDown)
		{
			elapsedMs_ -= static_cast<int>(delta);
			if (elapsedMs_ <= 0)
			{
				elapsedMs_ = 0;
				processEvent(TimerEvent::ReachTarget);
			}
		}
	}

	void startCounting(int targetMs, bool countUp)
	{
		targetMs_ = targetMs;
		countUp_ = countUp;
		elapsedMs_ = countUp ? 0 : targetMs;
		lastTick_ = Clock::now();
		processEvent(TimerEvent::Start);
	}

	void pause()
	{
		processEvent(TimerEvent::Pause);
	}

	void resume()
	{
		lastTick_ = Clock::now();
		processEvent(TimerEvent::Resume);
	}

	void reset()
	{
		elapsedMs_ = 0;
		processEvent(TimerEvent::Reset);
	}

	void clear()
	{
		elapsedMs_ = 0;
		processEvent(TimerEvent::Clear);
	}

	TimerState state() const { return current_; }
	int elapsed() const { return elapsedMs_; }
	int target() const { return targetMs_; }
	bool isCountingUp() const { return countUp_; }

	void setElapsed(int ms) { elapsedMs_ = ms; }

	float progress() const
	{
		if (targetMs_ <= 0) return 0.0f;
		return static_cast<float>(elapsedMs_) / static_cast<float>(targetMs_);
	}

	std::wstring formatTime() const
	{
		const int totalSec = elapsedMs_ / 1000;
		const int mins = totalSec / 60;
		const int secs = totalSec % 60;
		const int frac = (elapsedMs_ % 1000) / 100;

		wchar_t buf[16];
		swprintf_s(buf, L"%d.%02d%d", mins, secs, frac);
		return buf;
	}
};

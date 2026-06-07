#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <d2d1.h>
#include <dwrite.h>

enum class ClockState : std::uint8_t
{
	Running,
	Paused,
	Waiting,
	Dormant
};

class TimeTracker
{
private:
	using Clock = std::chrono::steady_clock;

	struct ClockData
	{
		ClockState phase = ClockState::Dormant;
		int ceiling = 0;
		int accumulated = 0;
		bool upward = false;
		Clock::time_point lastTick = Clock::now();
	};

	ClockData data_;

	static std::wstring formatMs(int totalMillis);
	int computeDelta(Clock::time_point now);

public:
	TimeTracker();

	ClockState fetchState() const;
	std::wstring renderText() const;
	int elapsedMs() const;
	int targetMs() const;
	bool countingUp() const;

	void setTargetMs(int millis);
	void setCountUp(bool enable);

	void startCount();
	void pauseCount();
	void clearElapsed();
	void advance();

	void paintTimer(
		ID2D1HwndRenderTarget* surface,
		IDWriteTextFormat* textFmt,
		D2D1_RECT_F bounds,
		ID2D1SolidColorBrush* ink,
		ID2D1SolidColorBrush* shadowInk = nullptr,
		float shadowDx = 0.0f,
		float shadowDy = 0.0f
	) const;
};

#include "TimeTracker.h"
#include <algorithm>
#include <cwchar>

namespace
{
	std::wstring padTwo(const int v)
	{
		return v < 10 ? L"0" + std::to_wstring(v) : std::to_wstring(v);
	}

	std::wstring padThree(const int v)
	{
		if (v < 10) return L"00" + std::to_wstring(v);
		if (v < 100) return L"0" + std::to_wstring(v);
		return std::to_wstring(v);
	}
}

TimeTracker::TimeTracker() = default;

std::wstring TimeTracker::formatMs(const int totalMillis)
{
	const int safe = (std::max)(totalMillis, 0);
	const int secs = safe / 1000;
	const int mins = secs / 60;
	const int rem = secs % 60;
	const std::wstring frac = padThree(safe % 1000);

	if (mins == 0) {
		return std::to_wstring(rem) + L"." + frac.substr(0, 2);
	}

	return std::to_wstring(mins) + L":" + padTwo(rem) + L"." + frac.substr(0, 1);
}

int TimeTracker::computeDelta(const Clock::time_point now)
{
	const int delta = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - data_.lastTick).count());
	data_.lastTick = now;
	return delta;
}

ClockState TimeTracker::fetchState() const
{
	return data_.phase;
}

std::wstring TimeTracker::renderText() const
{
	return formatMs(data_.accumulated);
}

int TimeTracker::elapsedMs() const
{
	return data_.accumulated;
}

int TimeTracker::targetMs() const
{
	return data_.ceiling;
}

bool TimeTracker::countingUp() const
{
	return data_.upward;
}

void TimeTracker::setTargetMs(const int millis)
{
	data_.ceiling = (std::max)(0, millis);
	clearElapsed();
}

void TimeTracker::setCountUp(const bool enable)
{
	if (data_.upward == enable) {
		return;
	}

	data_.upward = enable;
	clearElapsed();
}

void TimeTracker::startCount()
{
	if (data_.ceiling <= 0) {
		return;
	}

	const bool atBoundary =
		(data_.upward && data_.accumulated >= data_.ceiling) ||
		(!data_.upward && data_.accumulated <= 0);

	if (atBoundary) {
		clearElapsed();
	}

	data_.phase = ClockState::Running;
	data_.lastTick = Clock::now();
}

void TimeTracker::pauseCount()
{
	if (data_.phase != ClockState::Running) {
		return;
	}

	advance();
	if (data_.phase == ClockState::Running) {
		data_.phase = ClockState::Paused;
	}
}

void TimeTracker::clearElapsed()
{
	data_.accumulated = data_.upward ? 0 : data_.ceiling;
	data_.phase = ClockState::Dormant;
	data_.lastTick = Clock::now();
}

void TimeTracker::advance()
{
	if (data_.phase != ClockState::Running) {
		return;
	}

	const int elapsed = computeDelta(Clock::now());
	if (elapsed <= 0) {
		return;
	}

	if (data_.upward)
	{
		const int projected = data_.accumulated + elapsed;
		data_.accumulated = (std::min)(data_.ceiling, projected);
		if (data_.accumulated >= data_.ceiling) {
			data_.phase = ClockState::Waiting;
		}
		return;
	}

	const int remaining = data_.accumulated - elapsed;
	data_.accumulated = (std::max)(0, remaining);
	if (data_.accumulated <= 0) {
		data_.phase = ClockState::Waiting;
	}
}

void TimeTracker::paintTimer(
	ID2D1HwndRenderTarget* surface,
	IDWriteTextFormat* textFmt,
	const D2D1_RECT_F bounds,
	ID2D1SolidColorBrush* ink,
	ID2D1SolidColorBrush* shadowInk,
	const float shadowDx,
	const float shadowDy
) const
{
	if (surface == nullptr || textFmt == nullptr || ink == nullptr) {
		return;
	}

	const std::wstring label = renderText();
	const auto len = static_cast<UINT32>(wcslen(label.c_str()));

	if (shadowInk != nullptr && (shadowDx != 0.0f || shadowDy != 0.0f))
	{
		const D2D1_RECT_F shadowBounds = D2D1::RectF(
			bounds.left + shadowDx,
			bounds.top + shadowDy,
			bounds.right + shadowDx,
			bounds.bottom + shadowDy);
		surface->DrawTextW(label.c_str(), len, textFmt, shadowBounds, shadowInk);
	}

	surface->DrawTextW(label.c_str(), len, textFmt, bounds, ink);
}

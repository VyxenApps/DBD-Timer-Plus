#pragma once
#include <Windows.h>
#include <atomic>
#include <thread>
#include "AppConfig.h"

class OverlayWindow;
class ConfigStore;

// -------------------------------------------------------------------
// Orchestrator: Owns the app lifecycle, input relays, and tick pulse.
// -------------------------------------------------------------------
class Orchestrator
{
public:
	Orchestrator() = default;
	~Orchestrator();

	Orchestrator(const Orchestrator&) = delete;
	Orchestrator& operator=(const Orchestrator&) = delete;

	int boot(HINSTANCE instance, int showCmd);

	void postSignal();

	OverlayWindow* surface() const { return surface_; }
	ConfigStore* prefs() const { return prefs_; }

private:
	HINSTANCE appInstance_ = nullptr;
	OverlayWindow* surface_ = nullptr;
	ConfigStore* prefs_ = nullptr;

	std::atomic<bool> pulseActive_{ false };
	std::thread pulseThread_;

	HHOOK keyRelay_ = nullptr;
	HHOOK mouseRelay_ = nullptr;

	void attachHooks();
	void detachHooks();
	void beginPulse();
	void endPulse();

	void pulseThreadFn();
	void keyRelayFn(int code, WPARAM kind, LPARAM data);
	void mouseRelayFn(int code, WPARAM kind, LPARAM data);
	void padRelayFn(WORD mask);

	void applySettings();

	static Orchestrator* resolve(HWND h);

	friend LRESULT CALLBACK overlayDispatch(HWND, UINT, WPARAM, LPARAM);
};

void postClose();

extern OverlayWindow* overlayWindow_;

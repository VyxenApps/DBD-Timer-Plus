#include <windows.h>
#include <d2d1.h>
#include <string>
#include <thread>
#include <map>
#include <dwrite.h>
#include <objbase.h>
#include <commctrl.h>
#include "AppConfig.h"
#include "BrushFactory.h"
#include "JsonPersistence.h"
#include "NativeSurface.h"
#include "PrefsPanel.h"
#include "PalettePopup.h"
#include "SpinnerWheel.h"
#include "LoadoutEditor.h"
#include "NotifyBubble.h"
#include "OverlayWindow.h"
#include "EntryPoint.h"
#include "GamepadPoller.h"
#include "KeyRouter.h"
#include "ConfigStore.h"

#pragma comment(lib, "Msimg32.lib")
#pragma comment (lib, "d2d1")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "Xinput.lib")

using std::thread;
using std::wstring;

namespace
{
	constexpr int kPulseIntervalMs = 16;

	struct ModifierEquivalence
	{
		UINT sideA;
		UINT sideB;
		UINT unified;
	};

	UINT canonicalizeKey(UINT raw)
	{
		static const ModifierEquivalence equivalences[] = {
			{VK_LSHIFT,   VK_RSHIFT,   VK_SHIFT},
			{VK_LCONTROL, VK_RCONTROL, VK_CONTROL},
			{VK_LMENU,    VK_RMENU,    VK_MENU}
		};

		for (const auto& eq : equivalences)
		{
			if (raw == eq.sideA || raw == eq.sideB)
				return eq.unified;
		}
		return raw;
	}

	struct MouseButtonEntry
	{
		UINT firedMsg;
		UINT keyId;
	};

	UINT decodeMouseButton(const WPARAM msg, const MSLLHOOKSTRUCT* info)
	{
		static const MouseButtonEntry standardBtns[] = {
			{WM_MBUTTONDOWN, VK_MBUTTON},
			{WM_RBUTTONDOWN, VK_RBUTTON},
			{WM_LBUTTONDOWN, VK_LBUTTON}
		};

		for (const auto& btn : standardBtns)
		{
			if (msg == btn.firedMsg)
				return btn.keyId;
		}

		if (msg == WM_XBUTTONDOWN)
		{
			const WORD extra = HIWORD(info->mouseData);
			return (extra == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
		}

		return 0;
	}
}

AppConfig appSettings_;
HBRUSH paletteBrushes_[TINT_PALETTE_SZ];
HWND overlayHwnd_ = nullptr;
HINSTANCE hInstance_;
OverlayWindow* overlayWindow_ = nullptr;

namespace
{
	Orchestrator* g_orch = nullptr;
}

Orchestrator* Orchestrator::resolve(HWND h)
{
	return g_orch;
}

void postClose()
{
	if (g_orch)
	{
		auto* surf = g_orch->surface();
		if (surf) surf->running = false;
	}
	PostQuitMessage(0);
}

Orchestrator::~Orchestrator()
{
	endPulse();
	detachHooks();
}

void Orchestrator::attachHooks()
{
	keyRelay_ = SetWindowsHookEx(WH_KEYBOARD_LL, [](int code, WPARAM wp, LPARAM lp) -> LRESULT {
		if (g_orch) g_orch->keyRelayFn(code, wp, lp);
		return CallNextHookEx(nullptr, code, wp, lp);
	}, nullptr, NULL);

	mouseRelay_ = SetWindowsHookEx(WH_MOUSE_LL, [](int code, WPARAM wp, LPARAM lp) -> LRESULT {
		if (g_orch) g_orch->mouseRelayFn(code, wp, lp);
		return CallNextHookEx(nullptr, code, wp, lp);
	}, nullptr, NULL);
}

void Orchestrator::detachHooks()
{
	if (keyRelay_) { UnhookWindowsHookEx(keyRelay_); keyRelay_ = nullptr; }
	if (mouseRelay_) { UnhookWindowsHookEx(mouseRelay_); mouseRelay_ = nullptr; }
}

void Orchestrator::beginPulse()
{
	pulseActive_.store(true, std::memory_order_release);
	pulseThread_ = std::thread(&Orchestrator::pulseThreadFn, this);
}

void Orchestrator::endPulse()
{
	if (surface_) surface_->running = false;
	pulseActive_.store(false, std::memory_order_release);
	if (pulseThread_.joinable()) pulseThread_.join();
}

void Orchestrator::pulseThreadFn()
{
	while (pulseActive_.load(std::memory_order_acquire))
	{
		Sleep(kPulseIntervalMs);
		const HWND h = surface_ ? surface_->id() : nullptr;
		if (h) PostMessage(h, WM_TIMER_TICK, 0, 0);
	}
}

void Orchestrator::keyRelayFn(const int code, const WPARAM kind, const LPARAM data)
{
	if (code < 0 || kind != WM_KEYDOWN)
		return;

	const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(data);
	const UINT clean = canonicalizeKey(info->vkCode);

	KeyRouter::route(clean);
}

void Orchestrator::mouseRelayFn(const int code, const WPARAM kind, const LPARAM data)
{
	if (code < 0)
		return;

	if (kind != WM_LBUTTONDOWN && kind != WM_RBUTTONDOWN &&
		kind != WM_MBUTTONDOWN && kind != WM_XBUTTONDOWN)
		return;

	const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(data);
	const UINT button = decodeMouseButton(kind, info);

	if (button != 0)
		KeyRouter::route(button);
}

void Orchestrator::padRelayFn(const WORD mask)
{
	if (overlayHwnd_)
		SendMessage(overlayHwnd_, WM_PAD_EVENT, mask, NULL);
}

void Orchestrator::applySettings()
{
	if (!surface_) return;

	const BYTE alpha = appSettings_.flagSeeThrough ? 0 : 255;
	const DWORD layer = appSettings_.flagSeeThrough ? LWA_COLORKEY : LWA_ALPHA;
	SetLayeredWindowAttributes(overlayHwnd_, 0, alpha, layer);

	DWORD exStyle = static_cast<DWORD>(GetWindowLongPtr(overlayHwnd_, GWL_EXSTYLE));
	if (appSettings_.flagPassThru)
		exStyle |= WS_EX_TRANSPARENT;
	else
		exStyle &= ~WS_EX_TRANSPARENT;
	SetWindowLongPtr(overlayHwnd_, GWL_EXSTYLE, static_cast<LONG>(exStyle));

	KeyRouter::loadKeys(appSettings_);
}

int Orchestrator::boot(HINSTANCE hInst, const int showCmd)
{
	appInstance_ = hInst;
	hInstance_ = hInst;
	g_orch = this;

	prefs_ = new ConfigStore();
	prefs_->load(L"AppPrefs.json");
	if (!prefs_->exists())
		prefs_->createDefaults();
	appSettings_ = prefs_->get();
	appSettings_.flagTimerImg = false;

	auto* pad = new GamepadPoller();
	pad->setHandler([this](WORD m) { padRelayFn(m); });
	pad->beginPoll();

	OverlayWindow mainWin;
	PrefsPanel prefsDlg;
	PalettePopup colorPopup;
	SpinnerWheel spinWheel;
	LoadoutEditor survBuild;
	LoadoutEditor klrBuild;
	NotifyBubble toastBubble;

	prefsDlg.colorPicker = &colorPopup;
	prefsDlg.pSpinnerWheel = &spinWheel;
	prefsDlg.pSideSurvivorLoadoutEditor = &survBuild;
	prefsDlg.pSideKillerLoadoutEditor = &klrBuild;
	prefsDlg.chatOverlay = &toastBubble;
	mainWin.prefsDialog = &prefsDlg;

	overlayWindow_ = &mainWin;
	surface_ = &mainWin;

	if (!mainWin.spawn(0, 0, 285, 40,
		WS_EX_TOPMOST | WS_EX_LAYERED, WS_POPUP, nullptr, L"DBD Timer Plus+"))
	{
		return 0;
	}

	overlayHwnd_ = mainWin.id();

	SetLayeredWindowAttributes(mainWin.id(), 1, 255, LWA_COLORKEY | LWA_ALPHA);
	ShowWindow(mainWin.id(), showCmd);

	attachHooks();
	applySettings();
	toastBubble.applyConfig(appSettings_.notifyProfile);

	beginPulse();

	MSG msg = {};
	bool appRunning = true;
	while (appRunning)
	{
		BOOL result = PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
		if (result)
		{
			if (msg.message == WM_QUIT) { appRunning = false; break; }
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			WaitMessage();
		}
	}

	endPulse();
	pad->endPoll();
	delete pad;
	delete prefs_;
	RemoveFontResourceEx(L"FontsFree-Net-Anago2.ttf", FR_PRIVATE, nullptr);
	return static_cast<int>(msg.wParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nShowCmd)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	Orchestrator conductor;
	return conductor.boot(hInstance, nShowCmd);
}

#pragma once
#include "AppConfig.h"
#include "AppResources.h"
#include <Windows.h>
#include <CommCtrl.h>

// -------------------------------------------------------------------
// NativeFrame: Thin window host using GWLP_USERDATA dispatch.
// Dispatch via GWLP_USERDATA set during WM_CREATE
// during WM_CREATE via CREATESTRUCT::lpCreateParams.
// -------------------------------------------------------------------
class NativeFrame
{
protected:
	HWND nativeWindow_ = nullptr;
	int span_[2] = {};

	NativeFrame() = default;
	virtual ~NativeFrame() = default;

	NativeFrame(const NativeFrame&) = delete;
	NativeFrame& operator=(const NativeFrame&) = delete;

public:
	bool alive = false;
	bool running = false;

	HWND id() const { return nativeWindow_; }
	int wide() const { return span_[0]; }
	int tall() const { return span_[1]; }

	void finishWindowSetup()
	{
		tintFrame(nativeWindow_);
		silenceCaptionIcon(nativeWindow_);
	}

	BOOL spawn(
		int x = CW_USEDEFAULT,
		int y = CW_USEDEFAULT,
		int w = CW_USEDEFAULT,
		int h = CW_USEDEFAULT,
		DWORD exStyle = 0,
		DWORD style = WS_POPUP,
		HWND owner = nullptr,
		LPCWSTR title = nullptr,
		HMENU menu = nullptr,
		LPVOID extra = nullptr)
	{
		WNDCLASS wc = {};
		wc.lpfnWndProc = dispatch;
		wc.lpszClassName = identifyClass();
		wc.hInstance = hInstance_;
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1);
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

		if ((style & WS_CAPTION) == 0)
			wc.hIcon = LoadIcon(hInstance_, MAKEINTRESOURCE(IDI_ICON1));

		RegisterClass(&wc);

		DWORD adjEx = ((style & WS_CAPTION) != 0) ? (exStyle | WS_EX_DLGMODALFRAME) : exStyle;
		nativeWindow_ = CreateWindowEx(adjEx, identifyClass(), title, style, x, y, w, h, owner, menu, hInstance_, this);

		if (nativeWindow_ && wc.hIcon && (style & WS_CAPTION) == 0)
		{
			SendMessage(nativeWindow_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(wc.hIcon));
			SendMessage(nativeWindow_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(wc.hIcon));
		}

		span_[0] = w;
		span_[1] = h;

		return nativeWindow_ ? TRUE : FALSE;
	}

protected:
	virtual LPCWSTR identifyClass() const = 0;
	virtual LRESULT processMessage(UINT msg, WPARAM wp, LPARAM lp) = 0;

private:
	static void tintFrame(HWND w)
	{
		if (!w) return;
		HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
		if (!dwm) return;
		using SetAttr = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
		auto fn = reinterpret_cast<SetAttr>(GetProcAddress(dwm, "DwmSetWindowAttribute"));
		if (fn)
		{
			BOOL on = TRUE;
			fn(w, 20, &on, sizeof(on));
			fn(w, 19, &on, sizeof(on));
			COLORREF bg = RGB(45, 65, 88);
			COLORREF brd = RGB(29, 33, 41);
			COLORREF txt = RGB(238, 246, 255);
			fn(w, 34, &bg, sizeof(bg));
			fn(w, 35, &brd, sizeof(brd));
			fn(w, 36, &txt, sizeof(txt));
		}
		FreeLibrary(dwm);
	}

	static void silenceCaptionIcon(HWND w)
	{
		if (!w) return;
		if ((GetWindowLongPtr(w, GWL_STYLE) & WS_CAPTION) == 0) return;
		SendMessage(w, WM_SETICON, ICON_BIG, 0);
		SendMessage(w, WM_SETICON, ICON_SMALL, 0);
		SetClassLongPtr(w, GCLP_HICON, 0);
		SetClassLongPtr(w, GCLP_HICONSM, 0);
		LONG_PTR ex = GetWindowLongPtr(w, GWL_EXSTYLE);
		SetWindowLongPtr(w, GWL_EXSTYLE, ex | WS_EX_DLGMODALFRAME);
		SetWindowPos(w, nullptr, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	}

	static NativeFrame* retrieve(HWND h)
	{
		return reinterpret_cast<NativeFrame*>(GetWindowLongPtr(h, GWLP_USERDATA));
	}

	static LRESULT CALLBACK dispatch(HWND h, UINT m, WPARAM w, LPARAM l)
	{
		if (m == WM_CREATE)
		{
			auto* cs = reinterpret_cast<CREATESTRUCT*>(l);
			auto* frame = static_cast<NativeFrame*>(cs->lpCreateParams);
			frame->nativeWindow_ = h;
			SetWindowLongPtr(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(frame));
			return frame->processMessage(m, w, l);
		}

		auto* frame = retrieve(h);
		if (frame)
			return frame->processMessage(m, w, l);

		return DefWindowProc(h, m, w, l);
	}
};

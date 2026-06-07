#pragma once
#include <Windows.h>
#include <functional>
#include <unordered_map>

class Window
{
public:
	using Handler = std::function<LRESULT(WPARAM, LPARAM)>;

private:
	HWND hwnd_ = nullptr;
	std::unordered_map<UINT, Handler> dispatch_;
	ATOM classAtom_ = 0;
	bool registered_ = false;

	static LRESULT CALLBACK routeMessage(HWND idWin, UINT msg, WPARAM wp, LPARAM lp)
	{
		Window* self = nullptr;
		if (msg == WM_CREATE)
		{
			auto cs = reinterpret_cast<CREATESTRUCT*>(lp);
			self = static_cast<Window*>(cs->lpCreateParams);
			SetProp(idWin, L"WinPtr", reinterpret_cast<HANDLE>(self));
			self->hwnd_ = idWin;
		}
		else
		{
			self = reinterpret_cast<Window*>(GetProp(idWin, L"WinPtr"));
		}

		if (self)
		{
			auto it = self->dispatch_.find(msg);
			if (it != self->dispatch_.end())
				return it->second(wp, lp);
		}
		return DefWindowProc(idWin, msg, wp, lp);
	}

public:
	Window() = default;
	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;
	~Window() { destroy(); }

	void on(UINT message, Handler handler)
	{
		dispatch_[message] = std::move(handler);
	}

	bool create(DWORD exStyle, LPCWSTR className, LPCWSTR title, DWORD style,
		int x, int y, int w, int h, HWND parent, HMENU menu, HINSTANCE inst)
	{
		hwnd_ = CreateWindowExW(exStyle, className, title, style,
			x, y, w, h, parent, menu, inst, this);
		return hwnd_ != nullptr;
	}

	bool registerClass(HINSTANCE inst, LPCWSTR className, UINT style = 0,
		HICON icon = nullptr, HICON smallIcon = nullptr, HCURSOR cursor = nullptr,
		HBRUSH bg = nullptr, LPCWSTR menuName = nullptr)
	{
		WNDCLASSEXW wc = {};
		wc.cbSize = sizeof(wc);
		wc.style = style;
		wc.lpfnWndProc = routeMessage;
		wc.hInstance = inst;
		wc.lpszClassName = className;
		wc.hIcon = icon;
		wc.hIconSm = smallIcon;
		wc.hCursor = cursor;
		wc.hbrBackground = bg;
		wc.lpszMenuName = menuName;
		classAtom_ = RegisterClassExW(&wc);
		registered_ = (classAtom_ != 0);
		return registered_;
	}

	void show(int cmdShow) { ShowWindow(hwnd_, cmdShow); }
	void update() { UpdateWindow(hwnd_); }

	void destroy()
	{
		if (hwnd_)
		{
			DestroyWindow(hwnd_);
			hwnd_ = nullptr;
		}
		if (registered_)
		{
			UnregisterClass(MAKEINTATOM(classAtom_), GetModuleHandle(nullptr));
			registered_ = false;
		}
	}

	HWND handle() const { return hwnd_; }
	operator HWND() const { return hwnd_; }
	bool valid() const { return hwnd_ != nullptr; }

	void setLayered(COLORREF key, BYTE alpha, DWORD flags)
	{
		SetLayeredWindowAttributes(hwnd_, key, alpha, flags);
	}

	void setPosition(HWND insertAfter, int x, int y, int w, int h, UINT flags)
	{
		SetWindowPos(hwnd_, insertAfter, x, y, w, h, flags);
	}

	LONG_PTR getExStyle() const
	{
		return GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
	}

	void setExStyle(LONG_PTR style)
	{
		SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, style);
	}

	void invalidate(const RECT* rect = nullptr, BOOL erase = FALSE)
	{
		InvalidateRect(hwnd_, rect, erase);
	}

	void sendResize(int w, int h)
	{
	 SendMessage(hwnd_, WM_SIZE, 0, MAKELPARAM(w, h));
	}
};

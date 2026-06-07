#pragma once
#include "NativeSurface.h"

void setFontHeader(const HWND controlHandle);
void setFontChildren(const HWND parentHandle);
BOOL CALLBACK enumChildren(const HWND childHandle, const LPARAM fontParam);

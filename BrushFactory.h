#pragma once
#include "NativeSurface.h"

#define SAFE_RELEASE_OBJ(ppT) do { \
	if (*(ppT) != nullptr) { \
		(*(ppT))->Release(); \
		*(ppT) = nullptr; \
	} \
} while(0)

template <typename ComPtr>
inline void safeDiscard(ComPtr*& ptr)
{
	if (ptr)
	{
		ptr->Release();
		ptr = nullptr;
	}
}

HBITMAP loadResBitmap(int resourceId);
void initTintPalette();

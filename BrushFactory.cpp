#include "BrushFactory.h"

HBITMAP loadResBitmap(const int bitmap) {
	const HBITMAP hBitmap = LoadBitmap(GetModuleHandle(nullptr), MAKEINTRESOURCE(bitmap));

	return hBitmap;
}

void initTintPalette()
{
	constexpr COLORREF tones[TINT_PALETTE_SZ] = {
		// Paleta propia, colores únicos.
		RGB(180, 45, 67), RGB(220, 120, 50), RGB(195, 165, 80), RGB(170, 190, 60), RGB(130, 200, 90),
		RGB(40, 130, 160), RGB(60, 170, 210), RGB(120, 210, 230), RGB(80, 180, 130), RGB(50, 155, 100),
		RGB(90, 30, 110), RGB(120, 50, 160), RGB(150, 70, 200), RGB(180, 100, 230), RGB(210, 140, 255),
		RGB(240, 80, 200), RGB(200, 110, 220), RGB(160, 140, 240), RGB(100, 180, 255), RGB(50, 220, 240),
		RGB(20, 20, 25), RGB(45, 42, 50), RGB(95, 88, 98), RGB(170, 162, 175), RGB(225, 218, 230),

		// Temas del selector de color.
		RGB(248, 248, 252), RGB(235, 236, 240), RGB(210, 210, 215), RGB(180, 180, 188), RGB(140, 140, 150),
		RGB(88, 88, 100), RGB(72, 58, 74), RGB(60, 38, 55), RGB(48, 22, 40), RGB(38, 10, 30),
		RGB(218, 240, 248), RGB(182, 230, 244), RGB(145, 218, 238), RGB(96, 190, 230), RGB(38, 148, 220),
		RGB(22, 112, 190), RGB(14, 82, 162), RGB(8, 58, 130), RGB(5, 36, 98), RGB(3, 18, 68),
		RGB(248, 240, 188), RGB(245, 222, 135), RGB(238, 192, 68), RGB(218, 155, 25), RGB(192, 118, 0),
		RGB(158, 85, 0), RGB(122, 55, 0), RGB(90, 38, 0), RGB(68, 26, 0), RGB(48, 16, 0),
		RGB(245, 195, 195), RGB(240, 140, 150), RGB(228, 85, 110), RGB(208, 45, 90), RGB(185, 8, 85),
		RGB(155, 0, 85), RGB(122, 0, 85), RGB(92, 0, 78), RGB(70, 0, 62), RGB(50, 0, 44),
		RGB(205, 242, 198), RGB(155, 230, 152), RGB(98, 208, 105), RGB(58, 178, 80), RGB(18, 145, 58),
		RGB(10, 118, 52), RGB(7, 90, 46), RGB(4, 68, 40), RGB(2, 52, 35), RGB(0, 72, 48),

		// Paleta circular personalizada.
		RGB(135, 148, 168), RGB(85, 100, 122), RGB(32, 130, 230), RGB(75, 190, 210), RGB(88, 48, 195),
		RGB(0, 158, 215), RGB(235, 0, 110), RGB(60, 132, 212), RGB(68, 205, 88), RGB(75, 185, 205),
		RGB(85, 86, 235), RGB(25, 132, 230), RGB(218, 20, 140), RGB(128, 195, 65), RGB(28, 34, 48),
		RGB(248, 195, 15), RGB(238, 175, 38), RGB(245, 148, 20), RGB(230, 75, 95), RGB(230, 58, 56),
		RGB(185, 24, 44), RGB(235, 80, 142), RGB(205, 208, 212), RGB(94, 100, 112), RGB(220, 145, 25),
		RGB(40, 188, 205), RGB(45, 162, 90), RGB(46, 62, 95), RGB(46, 188, 130), RGB(34, 190, 80)
	};

	for (size_t idx = 0; idx < TINT_PALETTE_SZ; ++idx)
	{
		paletteBrushes_[idx] = CreateSolidBrush(tones[idx]);
	}
}

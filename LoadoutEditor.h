#pragma once
#include "NativeSurface.h"
#include "AppConfig.h"
#include <gdiplus.h>
#include <random>
#include <string>
#include <vector>

enum class LoadoutSide
{
	SideSurvivor,
	SideKiller
};

struct PerkIconData
{
	std::wstring displayName;
	std::wstring internalName;
	std::wstring filterToken;
	std::wstring characterLabel;
	std::wstring tooltipText;
	std::wstring filePath;
	Gdiplus::Image* loadedImage = nullptr;
	bool loadAttempted = false;
};

class LoadoutEditor : public NativeFrame
{
private:
	LoadoutSide currentSide = LoadoutSide::SideSurvivor;
	PerkSlot editingBuild = {};

	HWND hNameField_ = nullptr;
	HWND hNewBtn_ = nullptr;
	HWND hSaveBtn_ = nullptr;
	HWND hDeleteBtn_ = nullptr;
	HWND hRandomBtn_ = nullptr;
	HWND hSearchField_ = nullptr;

	HBRUSH backgroundBrush_ = nullptr;
	HBRUSH inputFieldBrush_ = nullptr;
	HFONT interfaceFont_ = nullptr;
	ULONG_PTR gdiToken_ = 0;

	Gdiplus::Bitmap* renderBuffer_ = nullptr;

	std::vector<PerkIconData> perkDb_;
	std::vector<int> filteredIdx_;
	std::vector<RECT> cardBounds_;
	std::array<RECT, 4> slotBounds_ = {};

	RECT popupArea_ = {};
	RECT searchArea_ = {};
	RECT listArea_ = {};
	RECT scrollbarArea_ = {};

	POINT cursorPosition_ = {};

	int selectedBuildIndex_ = -1;
	int shuffleHighlightIdx_ = -1;
	int shuffleFinalIdx_ = -1;
	int shuffleTickCount_ = 0;
	int shuffleTotalTicks_ = 0;
	int openPerkSlot_ = -1;
	int hoveredPerkIdx_ = -1;
	int listScrollTop_ = 0;

	int cachedBufW_ = 0;
	int cachedBufH_ = 0;

	bool isScrollDragging_ = false;
	bool shuffleAnimActive_ = false;
	int scrollOffset_ = 0;
	bool mouseLeftList_ = false;

	std::mt19937 rng_ = std::mt19937(std::random_device{}());

	void buildUI();
	void allocateGdi();
	void releaseGdi();
	void discoverPerkImages();
	void purgeAllPerkImages();
	void allocateBuffer(int w, int h);
	void computeWidgetLayout();
	void onPaint();
	void drawBuildCard(Gdiplus::Graphics& g, const RECT& rc);
	void drawBuildSlots(Gdiplus::Graphics& g, const RECT& rc);
	void drawPerkGrid(Gdiplus::Graphics& g);
	void drawPerkTooltip(Gdiplus::Graphics& g);
	void drawPerkGem(Gdiplus::Graphics& g, const RECT& rc, const std::wstring& path, bool selected, float borderWidth = 3.0f);
	void drawStyledButton(const DRAWITEMSTRUCT* di) const;
	void openPerkSelector(int slotIndex);
	void closePerkSelector();
	void applySearchFilter();
	void updateHoverState(POINT pt);
	void selectPerkAt(POINT pt);
	void scrollPerkList(int newTopIndex);
	void beginScrollDrag(int mouseY);
	void updateScrollDrag(int mouseY);
	void endScrollDrag();
	RECT computeScrollThumbRect() const;
	int countVisiblePerks() const;
	int computeMaxScrollTop() const;
	void selectBuild(int buildIndex);
	void startRandomShuffle();
	void advanceRandomShuffle();
	void resetRandomShuffle(bool clearResult);
	int countShuffleCandidates() const;
	void createNewBuild();
	void saveCurrentBuild();
	void deleteCurrentBuild();
	void readNameFieldValue();
	std::vector<PerkSlot>& getActiveRoleList();
	const std::vector<PerkSlot>& getActiveRoleList() const;
	PerkIconData* findPerkByPath(const std::wstring& path);
	std::wstring getSideLabel() const;
	std::wstring getSearchQuery() const;
	static std::wstring normalizePerkName(const std::wstring& fileName);

public:
	LoadoutEditor() = default;
	~LoadoutEditor();

	void showForSide(LoadoutSide side, HWND ownerWnd);

	LPCWSTR identifyClass() const override { return L"PerkGridFrame"; }
	LRESULT processMessage(UINT windowMessage, WPARAM wideParameter, LPARAM longParameter) override;
};

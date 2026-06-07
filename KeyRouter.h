#pragma once
#include <array>

#include "AppConfig.h"

class KeyRouter
{
public:
	struct BindSlot { int key; int action; };

private:
	static std::array<BindSlot, 16> slots_;
	static int slotCount_;

public:
	static void loadKeys(const AppConfig &config);
	static void loadDirect(int beginKey, int beginNrKey, int tmrOneKey, int tmrTwoKey,
		int padBeginKey, int padBeginNrKey, int padTmrOneKey, int padTmrTwoKey);
	static void route(int inputCode);
};

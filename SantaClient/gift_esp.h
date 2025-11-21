#pragma once
#include "global.h"
#include <vector>

constexpr int TOGGLE_KEY = VK_F1;

namespace gifts {

	struct Gift { Vec3 pos; float yaw; };
	extern std::vector<Gift> g_gifts;
	extern bool drawESP;

	void CheckToggleKey();

}
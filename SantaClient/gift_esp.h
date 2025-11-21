#pragma once
#include "global.h"
#include <vector>
#include <d3dx9.h>
constexpr int TOGGLE_KEY = VK_F1;

namespace gifts {

	struct Gift { Vec3 pos; float yaw; };
	extern bool drawESP;

	void CheckToggleKey();
	void DrawGiftESP(IDirect3DDevice9* dev);

}
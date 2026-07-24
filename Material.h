#pragma once

#include "Vector.h"

#include <cstdint>

// マテリアルを拡張する
struct Material {
	Vector4 color;
	int32_t enableLighting;

	// 0: Lambert、1: Half Lambert
	int32_t lightingMode;
};
#pragma once

#include "Vector.h"

#include <cstdint>

// マテリアルを拡張する
struct Material {
	Vector4 color;
	int32_t enableLighting;
};
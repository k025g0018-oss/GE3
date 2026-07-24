#pragma once

#include "Vector.h"
#include "Matrix3x3.h"

#include <cstdint>

// マテリアルを拡張する
struct Material {
	Vector4 color;
	int32_t enableLighting;
	Matrix3x3 uvTransform;

	// 0: Lambert、1: Half Lambert
	int32_t lightingMode;
};
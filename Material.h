#pragma once

#include "Vector.h"
#include "Matrix4x4.h"

#include <cstdint>

// マテリアルを拡張する
struct Material {
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;

	// 0: Lambert、1: Half Lambert
	int32_t lightingMode;
};
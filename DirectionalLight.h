#pragma once

#include "Vector.h"

// 平行光源の情報
struct DirectionalLight {
	Vector4 color;     // ライトの色
	Vector3 direction; // ライトが進む方向(単位ベクトル)
	float intensity;   // ライトの明るさ
};
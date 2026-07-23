#pragma once

#include "Matrix4x4.h"

// 頂点シェーダーへ送る座標変換行列
struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};
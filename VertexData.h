#pragma once

#include "Vector.h"

///// ----- VertexData ----- /////

/// --- 頂点データ ---
// 頂点データの拡張
struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal; // 頂点の法線方向
};
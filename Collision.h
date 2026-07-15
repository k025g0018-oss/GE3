#pragma once
#include "Vector.h" // 既存のVector構造体を利用
#include <cmath>

class Collision {
public:
    // 円同士の当たり判定
    static bool CheckCircleCollision(Vector3 pos1, float r1, Vector3 pos2, float r2) {
        float dx = pos2.x - pos1.x;
        float dy = pos2.y - pos1.y;
        float dz = pos2.z - pos1.z; // Z軸も考慮

        // 距離の二乗を計算(平方根計算は重いので、二乗同士で比較するのがコツ)
        float distSq = (dx * dx) + (dy * dy) + (dz * dz);
        float radiusSum = r1 + r2;

        // 半径の和の二乗より距離が小さければ衝突
        return distSq <= (radiusSum * radiusSum);
    }
};

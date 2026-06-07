#include "Vector.h"
#include <assert.h>
#include "Matrix4x4.h"

/// ---Vector3---
// 加算
Vector3 Vector3::operator+(const Vector3& obj) const {
	return { x + obj.x, y + obj.y, z + obj.z };
}

// 減算
Vector3 Vector3::operator-(const Vector3& obj) const {
	return { x - obj.x, y - obj.y, z - obj.z };
}

// スカラー倍
Vector3 Vector3::operator*(float scalar) const {
	return { x * scalar, y * scalar, z * scalar };
}

// 座標変換
Vector3 Vector3::Transform(const Vector3& vector, const Matrix4x4& matrix) {
	Vector3 result{};

	// (x, y, z, 1) * Matrix
	result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + 1.0f * matrix.m[3][0];
	result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + 1.0f * matrix.m[3][1];
	result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + 1.0f * matrix.m[3][2];

	// w成分の計算
	float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + 1.0f * matrix.m[3][3];

	// 同次座標のwで割る
	assert(w != 0.0f);
	result.x /= w;
	result.y /= w;
	result.z /= w;

	return result;
}
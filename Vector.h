#pragma once

// 前方宣言
struct Matrix4x4;

class Vector {
};

// ---Vector2---
struct Vector2 {
	float x, y;
};

// ---Vector3---
struct Vector3 {
	float x, y, z;

	// 演算子オーバーロードの宣言
	Vector3 operator+(const Vector3& obj) const;
	Vector3 operator-(const Vector3& obj) const;
	Vector3 operator*(float scalar) const;

	// ---静的関数の宣言---
	// 座標変換
	static Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);
};

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

// ---Vector4---
struct Vector4 {
	float x, y, z, w;
};
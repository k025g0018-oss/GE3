#pragma once

// 前方宣言
struct Matrix4x4;

#include "Vector.h"

struct Matrix4x4 {
	float m[4][4];

	/// ---静的関数の宣言---
	// 単位行列の作成
	static Matrix4x4 MakeIdentity();

	// x軸回転行列
	static Matrix4x4 MakeRotateXMatrix(float radian);
	// y軸回転行列
	static Matrix4x4 MakeRotateYMatrix(float radian);
	// z軸回転行列
	static Matrix4x4 MakeRotateZMatrix(float radian);

	// 平行移動行列
	static Matrix4x4 MakeTranslateMatrix(const Vector3& translate);
	// 拡大縮小行列
	static Matrix4x4 MakeScaleMatrix(const Vector3& scale);

	// Matrix4x4同士の掛け算
	static Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

	// 3次元アフィン変換行列
	static Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	// 逆行列
	static Matrix4x4 Inverse(const Matrix4x4& m);

	// 転置行列
	static Matrix4x4 Transpose(const Matrix4x4& m);

	// 単位行列の作成
	static Matrix4x4 MakeIdentity4x4();

	// 透視投影行列
	static Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

	// 正射影行列
	static Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

	// ビューポート変換行列
	Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);
};
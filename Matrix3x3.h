#pragma once

#include "Vector.h"

#include <cmath>

struct Matrix3x3 {
	// 4列目はHLSLのConstant Bufferへ合わせるための余白
	float m[3][4]; // HLSLのrow_major float3x3と配置を合わせるため、各行を16バイトにする

	// 単位行列を作成する
	static Matrix3x3 MakeIdentity();

	// 2つの3x3行列を掛け合わせる
	static Matrix3x3 Multiply(
		const Matrix3x3& matrix1,
		const Matrix3x3& matrix2
	);

	// 2次元の平行移動行列を作成する
	static Matrix3x3 MakeTranslateMatrix(
		const Vector2& translate
	);

	// 2次元の回転行列を作成する
	static Matrix3x3 MakeRotateMatrix(float theta);

	// 2次元の拡大縮小行列を作成する
	static Matrix3x3 MakeScaleMatrix(
		const Vector2& scale
	);

	// Scale、Rotate、Translateの順番で合成する
	static Matrix3x3 MakeAffineMatrix(
		const Vector2& scale,
		float rotate,
		const Vector2& translate
	);
};
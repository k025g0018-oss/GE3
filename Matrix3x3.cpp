#include "Matrix3x3.h"

// 単位行列を作成する
Matrix3x3 Matrix3x3::MakeIdentity() {
	Matrix3x3 result{};

	result.m[0][0] = 1.0f;
	result.m[1][1] = 1.0f;
	result.m[2][2] = 1.0f;

	return result;
}

// 2つの3x3行列を掛け合わせる
Matrix3x3 Matrix3x3::Multiply(
	const Matrix3x3& matrix1,
	const Matrix3x3& matrix2
) {
	Matrix3x3 result{};

	// 行列として使用する3行3列だけを計算する
	for (int row = 0; row < 3; ++row) {
		for (int column = 0; column < 3; ++column) {
			for (int index = 0; index < 3; ++index) {
				result.m[row][column] +=
					matrix1.m[row][index] *
					matrix2.m[index][column];
			}
		}
	}

	return result;
}

// 2次元の平行移動行列を作成する
Matrix3x3 Matrix3x3::MakeTranslateMatrix(
	const Vector2& translate
) {
	Matrix3x3 result = MakeIdentity();

	// 行ベクトル方式なので、平行移動は3行目に設定する
	result.m[2][0] = translate.x;
	result.m[2][1] = translate.y;

	return result;
}

// 2次元の回転行列を作成する
Matrix3x3 Matrix3x3::MakeRotateMatrix(float theta) {
	Matrix3x3 result{};

	const float cosine = std::cos(theta);
	const float sine = std::sin(theta);

	// 既存のMatrix4x4と同じ行ベクトル方式の回転行列
	result.m[0][0] = cosine;
	result.m[0][1] = sine;

	result.m[1][0] = -sine;
	result.m[1][1] = cosine;

	result.m[2][2] = 1.0f;

	return result;
}

// 2次元の拡大縮小行列を作成する
Matrix3x3 Matrix3x3::MakeScaleMatrix(
	const Vector2& scale
) {
	Matrix3x3 result{};

	result.m[0][0] = scale.x;
	result.m[1][1] = scale.y;
	result.m[2][2] = 1.0f;

	return result;
}

// Scale、Rotate、Translateの順番で合成する
Matrix3x3 Matrix3x3::MakeAffineMatrix(
	const Vector2& scale,
	float rotate,
	const Vector2& translate
) {
	const Matrix3x3 scaleMatrix =
		MakeScaleMatrix(scale);

	const Matrix3x3 rotateMatrix =
		MakeRotateMatrix(rotate);

	const Matrix3x3 translateMatrix =
		MakeTranslateMatrix(translate);

	// UVへScale、Rotate、Translateの順番で適用する
	return Multiply(
		scaleMatrix,
		Multiply(
		rotateMatrix,
		translateMatrix
	)
	);
}


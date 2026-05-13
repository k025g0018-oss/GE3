#include "Matrix4x4.h"
#include <cmath>

// 単位行列の作成
Matrix4x4 Matrix4x4::MakeIdentity() {
	Matrix4x4 result;
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			if (i == j) {
				result.m[i][j] = 1.0f; // 対角成分は1
			} else {
				result.m[i][j] = 0.0f; // それ以外は0
			}
		}
	}
	return result;
}

// x軸回転行列
Matrix4x4 Matrix4x4::MakeRotateXMatrix(float radian) {
	float s = std::sin(radian);
	float c = std::cos(radian);

	return{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, c, s, 0.0f,
		0.0f, -s, c, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
}

// y軸回転行列
Matrix4x4 Matrix4x4::MakeRotateYMatrix(float radian) {
	float s = std::sin(radian);
	float c = std::cos(radian);

	return{
		c, 0.0f, -s, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		s, 0.0f, c, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
}

// z軸回転行列
Matrix4x4 Matrix4x4::MakeRotateZMatrix(float radian) {
	float s = std::sin(radian);
	float c = std::cos(radian);

	return{
		c, s, 0.0f, 0.0f,
		-s, c, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
}

// 平行移動行列
Matrix4x4 Matrix4x4::MakeTranslateMatrix(const Vector3& translate) {
	Matrix4x4 result{};

	// 単位行列
	result.m[0][0] = 1.0f;
	result.m[1][1] = 1.0f;
	result.m[2][2] = 1.0f;
	result.m[3][3] = 1.0f;

	// 4行目に移動量
	result.m[3][0] = translate.x;
	result.m[3][1] = translate.y;
	result.m[3][2] = translate.z;

	return result;
}

// 拡大縮小行列
Matrix4x4 Matrix4x4::MakeScaleMatrix(const Vector3& scale) {
	Matrix4x4 result{};

	// 対角に
	result.m[0][0] = scale.x;
	result.m[1][1] = scale.y;
	result.m[2][2] = scale.z;
	result.m[3][3] = 1.0f;

	return result;
}

// Matrix4x4同士の掛け算
Matrix4x4 Matrix4x4::Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result{};
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			for (int k = 0; k < 4; k++) {
				result.m[i][j] += m1.m[i][k] * m2.m[k][j];
			}
		}
	}
	return result;
}

// 3次元アフィン変換行列
Matrix4x4 Matrix4x4:: MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
	Matrix4x4 result{};

	// 各成分の行列を作成
	Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);

	Matrix4x4 rotateXMatrix = MakeRotateXMatrix(rotate.x);
	Matrix4x4 rotateYMatrix = MakeRotateYMatrix(rotate.y);
	Matrix4x4 rotateZMatrix = MakeRotateZMatrix(rotate.z);

	Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

	// 回転行列の合成
	Matrix4x4 rotateMatrix = Multiply(rotateXMatrix, Multiply(rotateYMatrix, rotateZMatrix));

	// SRTの順番で行列を合成
	result = Multiply(scaleMatrix, Multiply(rotateMatrix, translateMatrix));

	return result;
}

// 逆行列
Matrix4x4 Matrix4x4::Inverse(const Matrix4x4& m) {
	Matrix4x4 r{};

	float a11 = m.m[0][0], a12 = m.m[0][1], a13 = m.m[0][2], a14 = m.m[0][3],
		a21 = m.m[1][0], a22 = m.m[1][1], a23 = m.m[1][2], a24 = m.m[1][3],
		a31 = m.m[2][0], a32 = m.m[2][1], a33 = m.m[2][2], a34 = m.m[2][3],
		a41 = m.m[3][0], a42 = m.m[3][1], a43 = m.m[3][2], a44 = m.m[3][3];

	// 逆列式 |A|
	float det =
		a11 * a22 * a33 * a44 +
		a11 * a23 * a34 * a42 +
		a11 * a24 * a32 * a43 -
		a11 * a24 * a33 * a42 -
		a11 * a23 * a32 * a44 -
		a11 * a22 * a34 * a43 -
		a12 * a21 * a33 * a44 -
		a13 * a21 * a34 * a42 -
		a14 * a21 * a32 * a43 +
		a14 * a21 * a33 * a42 +
		a13 * a21 * a32 * a44 +
		a12 * a21 * a34 * a43 +
		a12 * a23 * a31 * a44 +
		a13 * a24 * a31 * a42 +
		a14 * a22 * a31 * a43 -
		a14 * a23 * a31 * a42 -
		a13 * a22 * a31 * a44 -
		a12 * a24 * a31 * a43 -
		a12 * a23 * a34 * a41 -
		a13 * a24 * a32 * a41 -
		a14 * a22 * a33 * a41 +
		a14 * a23 * a32 * a41 +
		a13 * a22 * a34 * a41 +
		a12 * a24 * a33 * a41;

	// 逆行列が存在しないとき
	if (det == 0.0f) {
		return r;
	}

	float invDet = 1.0f / det;

	r.m[0][0] = (
		a22 * a33 * a44 +
		a23 * a34 * a42 +
		a24 * a32 * a43 -
		a24 * a33 * a42 -
		a23 * a32 * a44 -
		a22 * a34 * a43
		) * invDet;

	r.m[0][1] = (
		-a12 * a33 * a44 -
		a13 * a34 * a42 -
		a14 * a32 * a43 +
		a14 * a33 * a42 +
		a13 * a32 * a44 +
		a12 * a34 * a43
		) * invDet;

	r.m[0][2] = (
		a12 * a23 * a44 +
		a13 * a24 * a42 +
		a14 * a22 * a43 -
		a14 * a23 * a42 -
		a13 * a22 * a44 -
		a12 * a24 * a43
		) * invDet;

	r.m[0][3] = (
		-a12 * a23 * a34 -
		a13 * a24 * a32 -
		a14 * a22 * a33 +
		a14 * a23 * a32 +
		a13 * a22 * a34 +
		a12 * a24 * a33
		) * invDet;

	r.m[1][0] = (
		-a21 * a33 * a44 -
		a23 * a34 * a41 -
		a24 * a31 * a43 +
		a24 * a33 * a41 +
		a23 * a31 * a44 +
		a21 * a34 * a43
		) * invDet;

	r.m[1][1] = (
		a11 * a33 * a44 +
		a13 * a34 * a41 +
		a14 * a31 * a43 -
		a14 * a33 * a41 -
		a13 * a31 * a44 -
		a11 * a34 * a43
		) * invDet;

	r.m[1][2] = (
		-a11 * a23 * a44 -
		a13 * a24 * a41 -
		a14 * a21 * a43 +
		a14 * a23 * a41 +
		a13 * a21 * a44 +
		a11 * a24 * a43
		) * invDet;

	r.m[1][3] = (
		a11 * a23 * a34 +
		a13 * a24 * a31 +
		a14 * a21 * a33 -
		a14 * a23 * a31 -
		a13 * a21 * a34 -
		a11 * a24 * a33
		) * invDet;

	r.m[2][0] = (
		a21 * a32 * a44 +
		a22 * a34 * a41 +
		a24 * a31 * a42 -
		a24 * a32 * a41 -
		a22 * a31 * a44 -
		a21 * a34 * a42
		) * invDet;

	r.m[2][1] = (
		-a11 * a32 * a44 -
		a12 * a34 * a41 -
		a14 * a31 * a42 +
		a14 * a32 * a41 +
		a12 * a31 * a44 +
		a11 * a34 * a42
		) * invDet;

	r.m[2][2] = (
		a11 * a22 * a44 +
		a12 * a24 * a41 +
		a14 * a21 * a42 -
		a14 * a22 * a41 -
		a12 * a21 * a44 -
		a11 * a24 * a42
		) * invDet;


	r.m[2][3] = (
		-a11 * a22 * a34 -
		a12 * a24 * a31 -
		a14 * a21 * a32 +
		a14 * a22 * a31 +
		a12 * a21 * a34 +
		a11 * a24 * a32
		) * invDet;

	r.m[3][0] = (
		-a21 * a32 * a43 -
		a22 * a33 * a41 -
		a23 * a31 * a42 +
		a23 * a32 * a41 +
		a22 * a31 * a43 +
		a21 * a33 * a42
		) * invDet;

	r.m[3][1] = (
		a11 * a32 * a43 +
		a12 * a33 * a41 +
		a13 * a31 * a42 -
		a13 * a32 * a41 -
		a12 * a31 * a43 -
		a11 * a33 * a42
		) * invDet;

	r.m[3][2] = (
		-a11 * a22 * a43 -
		a12 * a23 * a41 -
		a13 * a21 * a42 +
		a13 * a22 * a41 +
		a12 * a21 * a43 +
		a11 * a23 * a42
		) * invDet;

	r.m[3][3] = (
		a11 * a22 * a33 +
		a12 * a23 * a31 +
		a13 * a21 * a32 -
		a13 * a22 * a31 -
		a12 * a21 * a33 -
		a11 * a23 * a32
		) * invDet;

	return r;
}

// 転置行列
Matrix4x4 Matrix4x4::Transpose(const Matrix4x4& m) {
	Matrix4x4 result{};
	// 行と列のインデックスを入れ替える
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			result.m[i][j] = m.m[j][i];
		}
	}
	return result;
}

// 単位行列の作成
Matrix4x4 Matrix4x4::MakeIdentity4x4() {
	Matrix4x4 result;
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			if (i == j) {
				result.m[i][j] = 1.0f; // 対角成分は1
			} else {
				result.m[i][j] = 0.0f; // それ以外は0
			}
		}
	}
	return result;
}

// 透視投影行列
Matrix4x4 Matrix4x4::MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
	Matrix4x4 result{};

	// コンタンジェントを計算 ( 1 / tan(fovY / 2) )
	float cot = 1.0f / std::tan(fovY / 2.0f);

	// tanθ = a/b, cotθ = 1 / tanθ = b/a, y * cotθ -> a * b/a = b

	// result.m[0][0] = (1.0f / aspectRatio) * (fovY / 2.0f);
	// result.m[1][1] = fovY / 2.0f;

	result.m[0][0] = cot / aspectRatio;
	result.m[0][1] = 0.0f;
	result.m[0][2] = 0.0f;
	result.m[0][3] = 0.0f;
	result.m[1][0] = 0.0f;
	result.m[1][1] = cot;
	result.m[1][2] = 0.0f;
	result.m[1][3] = 0.0f;
	result.m[2][0] = 0.0f;
	result.m[2][1] = 0.0f;
	result.m[2][2] = farClip / (farClip - nearClip);
	result.m[2][3] = 1.0f;
	result.m[3][0] = 0.0f;
	result.m[3][1] = 0.0f;
	result.m[3][2] = -(nearClip * farClip) / (farClip - nearClip);
	result.m[3][3] = 0.0f;

	return result;
}

// 正射影行列
Matrix4x4 Matrix4x4::MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
	Matrix4x4 result{};

	result.m[0][0] = 2.0f / (right - left);
	result.m[0][1] = 0.0f;
	result.m[0][2] = 0.0f;
	result.m[0][3] = 0.0f;
	result.m[1][0] = 0.0f;
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[1][2] = 0.0f;
	result.m[1][3] = 0.0f;
	result.m[2][0] = 0.0f;
	result.m[2][1] = 0.0f;
	result.m[2][2] = 1.0f / (farClip - nearClip);
	result.m[2][3] = 0.0f;
	result.m[3][0] = (left + right) / (left - right);
	result.m[3][1] = (top + bottom) / (bottom - top);
	result.m[3][2] = nearClip / (nearClip - farClip);
	result.m[3][3] = 1.0f;

	return result;
}

// ビューポート変換行列
Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
	Matrix4x4 result{};

	result.m[0][0] = width / 2.0f;
	result.m[0][1] = 0.0f;
	result.m[0][2] = 0.0f;
	result.m[0][3] = 0.0f;
	result.m[1][0] = 0.0f;
	result.m[1][1] = -height / 2.0f;
	result.m[1][2] = 0.0f;
	result.m[1][3] = 0.0f;
	result.m[2][0] = 0.0f;
	result.m[2][1] = 0.0f;
	result.m[2][2] = maxDepth - minDepth;
	result.m[2][3] = 0.0f;
	result.m[3][0] = left + (width / 2.0f);
	result.m[3][1] = top + (height / 2.0f);
	result.m[3][2] = minDepth;
	result.m[3][3] = 1.0f;

	return result;
}
#pragma once

#include "Matrix4x4.h"
#include "Vector.h"

class DebugCamera {
public:
	// コンストラクタ
	DebugCamera();
	// デストラクタ
	~DebugCamera();

	///// ----- 初期化処理 ----- /////

	// 画面の縦横比を受け取り、カメラ行列を初期化する
	void Initialize(float aspectRatio);

	///// ----- 更新処理 ----- /////

	// 座標と回転角からView・Projection行列を更新する
	void Update();

	///// ----- マウス操作 ----- /////

	// 左ドラッグ量をX・Y軸回転へ反映する
	void RotateByMouse(const Vector2& mouseDelta);
	// 右ドラッグ量をカメラ基準の上下左右移動へ反映する
	void MoveByMouse(const Vector2& mouseDelta);
	// ホイール量をカメラ基準の前後移動へ反映する
	void MoveForwardByMouse(float wheel);

	///// ----- Getter ----- /////

	// 3DオブジェクトのWVP計算へ渡す行列を取得する
	const Matrix4x4& GetViewMatrix() const {
		// 行列をコピーせずconst参照で返す
		return viewMatrix_;
	}

	const Matrix4x4& GetProjectionMatrix() const {
		// 行列をコピーせずconst参照で返す
		return projectionMatrix_;
	}

private:
	///// ----- メンバ変数 ----- /////

	// X, Y, Z軸回りのローカル回転角
	// Vector3 rotation_ = {0.0f, 0.0f, 0.0f};
	// 累積回転行列
	Matrix4x4 matRot_;

	// ピボット回転の中心座標
	Vector3 target_ = {
		0.0f,
		0.0f,
		0.0f
	};

	// ターゲットからカメラまでの距離
	float distance_ = nullptr;

	// ローカル座標
	Vector3 translation_ = {0.0f, 0.0f, -50.0f};
	// ビュー行列
	Matrix4x4 viewMatrix_{};
	// 射影行列
	Matrix4x4 projectionMatrix_{};

	// 透視投影行列を作成するための設定値
	float aspectRatio_ = 1.0f;
	float fovY_ = 0.45f;
	float nearClip_ = 0.1f;
	float farClip_ = 100.0f;

	// マウスの入力値へ掛ける操作速度
	float rotateSpeed_ = 0.005f;
	float moveSpeed_ = 0.01f;
	float forwardSpeed_ = 0.3f;
};


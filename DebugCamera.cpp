#include "DebugCamera.h"

#include <algorithm>

// コンストラクタ
DebugCamera::DebugCamera() = default;
// デストラクタ
DebugCamera::~DebugCamera() = default;

///// ----- 初期化処理 ----- /////

void DebugCamera::Initialize(float aspectRatio) {
	// Projection行列へ使用する画面の縦横比を保存する
	aspectRatio_ = aspectRatio;

	// 累積回転行列は単位行列から開始する
	matRot_ = Matrix4x4::MakeIdentity4x4();

	// 原点をピボット回転の中心にする
	target_ = {
		0.0f,
		0.0f,
		0.0f
	};

	// ターゲットからカメラまでの初期距離
	distance_ = 50.0f;

	// 初期座標と初期角度から行列を作成する
	Update();
}

///// ----- 更新処理 ----- /////

void DebugCamera::Update() {
	/*
	// X、Y、Z軸の回転行列を作成する
	const Matrix4x4 rotateXMatrix =
		Matrix4x4::MakeRotateXMatrix(rotation_.x);
	const Matrix4x4 rotateYMatrix =
		Matrix4x4::MakeRotateYMatrix(rotation_.y);
	const Matrix4x4 rotateZMatrix =
		Matrix4x4::MakeRotateZMatrix(rotation_.z);
	*/

	// 追加回転分の回転行列を作成
	Matrix4x4 matRotDelta = Matrix4x4::MakeIdentity();
	matRotDelta *= Matrix4x4::MakeRotateXMatrix(今回のX軸回転角度);
	matRotDelta *= Matrix4x4::MakeRotateYMatrix();

	// 各軸の回転を1つの回転行列へまとめる
	const Matrix4x4 rotateMatrix =
		Matrix4x4::Multiply(
			rotateXMatrix,
			Matrix4x4::Multiply(
			rotateYMatrix,
			rotateZMatrix)
		);

	// カメラの座標から平行移動行列を作成する
	const Matrix4x4 translateMatrix =
		Matrix4x4::MakeTranslateMatrix(translation_);

	// 回転行列と平行移動行列からカメラのWorld行列を作成する
	const Matrix4x4 cameraWorldMatrix =
		Matrix4x4::Multiply(
			rotateMatrix,
			translateMatrix
		);

	// カメラのWorld行列の逆行列をView行列にする
	viewMatrix_ =
		Matrix4x4::Inverse(cameraWorldMatrix);

	// 画角や縦横比から透視投影行列を作成する
	projectionMatrix_ =
		Matrix4x4::MakePerspectiveFovMatrix(
			fovY_,
			aspectRatio_,
			nearClip_,
			farClip_
		);
}

///// ----- マウス操作 ----- /////

// 左ドラッグ量をX・Y軸回転へ反映する
void DebugCamera::RotateByMouse(
	const Vector2& mouseDelta) {

	// マウスの横移動をY軸回転へ反映する
	rotation_.y +=
		mouseDelta.x * rotateSpeed_;

	// マウスの縦移動をX軸回転へ反映する
	rotation_.x +=
		mouseDelta.y * rotateSpeed_;

	// 真上・真下を越えて操作が反転することを防ぐ
	rotation_.x = (std::clamp)(
			rotation_.x,
			-1.45f,
			1.45f
			);
}

// 右ドラッグ量をカメラ基準の上下左右移動へ反映する
void DebugCamera::MoveByMouse(const Vector2& mouseDelta) {
	// 現在のカメラ角度から回転行列を作成する
	const Matrix4x4 rotateMatrix =
		Matrix4x4::Multiply(
			Matrix4x4::MakeRotateXMatrix(rotation_.x),
			Matrix4x4::Multiply(
			Matrix4x4::MakeRotateYMatrix(rotation_.y),
			Matrix4x4::MakeRotateZMatrix(rotation_.z)));

	// マウスの移動量をカメラのローカル移動量へ変換する
	Vector3 move = {
		-mouseDelta.x * moveSpeed_,
		mouseDelta.y * moveSpeed_,
		0.0f
	};

	// カメラの角度に合わせて移動方向を回転する
	move =
		Vector3::Transform(
			move,
			rotateMatrix);

	// 回転後の移動量をカメラ座標へ加算する
	translation_ =
		translation_ + move;
}

// ホイール量をカメラ基準の前後移動へ反映する
void DebugCamera::MoveForwardByMouse(float wheel) {
	// 現在のカメラ角度から回転行列を作成する
	const Matrix4x4 rotateMatrix =
		Matrix4x4::Multiply(
			Matrix4x4::MakeRotateXMatrix(rotation_.x),
			Matrix4x4::Multiply(
			Matrix4x4::MakeRotateYMatrix(rotation_.y),
			Matrix4x4::MakeRotateZMatrix(rotation_.z)));

	// ホイール量からカメラのローカル前後移動量を作成する
	Vector3 move = {
		0.0f,
		0.0f,
		wheel * forwardSpeed_
	};

	// カメラの角度に合わせて前後方向を回転する
	move =
		Vector3::Transform(
			move,
			rotateMatrix);

	// 回転後の移動量をカメラ座標へ加算する
	translation_ =
		translation_ + move;
}
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

	// ピボット回転を初期状態へ戻して行列を作成する
	Reset();
}

///// ----- リセット ----- /////

void DebugCamera::Reset() {
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

	// リセットした値からView・Projection行列を作り直す
	Update();
}

///// ----- 更新処理 ----- /////

void DebugCamera::Update() {
	// ターゲットから見たカメラの初期相対座標を作る
	Vector3 cameraOffset = {
		0.0f,
		0.0f,
		-distance_
	};

	// 累積回転行列で相対座標を回転させる
	cameraOffset =
		Vector3::Transform(
			cameraOffset,
			matRot_);

	// ターゲット座標へ回転後の相対座標を足してカメラ座標を求める
	translation_ =
		target_ + cameraOffset;

	// 計算したカメラ座標から平行移動行列を作成する
	const Matrix4x4 translateMatrix = Matrix4x4::MakeTranslateMatrix(translation_);

	// 累積回転行列と平行移動行列からカメラのWorld行列を作成する
	const Matrix4x4 cameraWorldMatrix =
		Matrix4x4::Multiply(
			matRot_,
			translateMatrix
		);

	// カメラのWorld行列の逆行列をView行列にする
	viewMatrix_ = Matrix4x4::Inverse(cameraWorldMatrix);

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
void DebugCamera::RotateByMouse(const Vector2& mouseDelta) {
	// 今回のマウス移動量から追加するX軸回転角を計算する
	const float deltaRotateX = mouseDelta.y * rotateSpeed_;

	// 今回のマウス移動量から追加するY軸回転角を計算する
	const float deltaRotateY = mouseDelta.x * rotateSpeed_;

	// 今回追加するX軸回転行列を作成する
	const Matrix4x4 matRotX = Matrix4x4::MakeRotateXMatrix(deltaRotateX);

	// 今回追加するY軸回転行列を作成する
	const Matrix4x4 matRotY = Matrix4x4::MakeRotateYMatrix(deltaRotateY);

	// X軸とY軸の追加回転を1つの行列へまとめる
	const Matrix4x4 matRotDelta = Matrix4x4::Multiply(matRotX, matRotY);

	// 今回の回転を以前までの累積回転へ合成する
	matRot_ = Matrix4x4::Multiply(matRotDelta, matRot_);
}

// 右ドラッグ量をカメラ基準の上下左右移動へ反映する
void DebugCamera::MoveByMouse(const Vector2& mouseDelta) {
	// マウス移動量からカメラ基準の平行移動量を作る
	Vector3 move = {
		-mouseDelta.x * moveSpeed_,
		mouseDelta.y * moveSpeed_,
		0.0f
	};

	// カメラの角度に合わせて移動方向を回転する
	move =
		Vector3::Transform(
			move,
			matRot_
		);

	// 回転後の移動量をカメラ座標へ加算する
	target_ = target_ + move;
}

// ホイール量をカメラ基準の前後移動へ反映する
void DebugCamera::MoveForwardByMouse(float wheel) {
	// ホイール入力でターゲットとの距離を変更する
	distance_ -=
		wheel * forwardSpeed_;

	// カメラがターゲットを通り越さないよう最小距離を設定する
	distance_ =
		(std::max)(
			distance_,
			0.1f);
}

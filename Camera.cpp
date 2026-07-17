#include "Camera.h"

#include <algorithm>

/// <summary>
/// 初期化処理
/// </summary>
/// <param name="aspectRatio"></param>
void Camera::Initialize(float aspectRatio) {
	// Sceneの縦横比を保持してProjection行列へ使用する
	aspectRatio_ = aspectRatio;
	// 初期Transformへ戻してから行列を作成する
	Reset();
	Update();
}

/// <summary>
/// 更新処理
/// </summary>
void Camera::Update() {
	// CameraのTransformからCamera行列を作成する
	const Matrix4x4 cameraMatrix =
		Matrix4x4::MakeAffineMatrix(
			transform_.scale,
			transform_.rotate,
			transform_.translate
		);

	// Camera行列の逆行列がView行列になる
	viewMatrix_ =
		Matrix4x4::Inverse(cameraMatrix);

	// 透視投影行列を更新する
	projectionMatrix_ =
		Matrix4x4::MakePerspectiveFovMatrix(
			fovY_,
			aspectRatio_,
			nearClip_,
			farClip_
		);
}

void Camera::RotateByMouse(
	const Vector2& mouseDelta
) {
	// 横方向のドラッグをY軸回転へ反映する
	transform_.rotate.y +=
		mouseDelta.x * rotateSpeed_;

	// 縦方向のドラッグをX軸回転へ反映する
	transform_.rotate.x +=
		mouseDelta.y * rotateSpeed_;

	// 真上や真下を向いて操作が反転することを防ぐ
	transform_.rotate.x =
		(std::clamp)(
			transform_.rotate.x,
			-1.45f,
			1.45f
			);
}

void Camera::MoveByMouse(
	const Vector2& mouseDelta
) {
	// 現在はMT3と同じくワールドXY方向へ平行移動する
	transform_.translate.x -=
		mouseDelta.x * moveSpeed_;

	transform_.translate.y +=
		mouseDelta.y * moveSpeed_;
}

void Camera::ZoomByMouse(float wheel) {
	// 現在の座標系ではZを増減して前後移動する
	transform_.translate.z +=
		wheel * zoomSpeed_;
}

/// <summary>
/// リセット
/// </summary>
void Camera::Reset() {
	// 現在のオブジェクトが画面全体へ広がらないよう、Z=-10を初期位置にする
	transform_ = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, -10.0f}
	};
}

Transform& Camera::GetTransform() {
	// Propertiesから直接編集するため参照を返す
	return transform_;
}

const Transform& Camera::GetTransform() const {
	return transform_;
}

const Matrix4x4& Camera::GetViewMatrix() const {
	// 行列のコピーを避けるためconst参照で返す
	return viewMatrix_;
}

const Matrix4x4& Camera::GetProjectionMatrix() const {
	return projectionMatrix_;
}

void Camera::SetAspectRatio(float aspectRatio) {
	// 高さ0による不正なProjection行列を防ぐ
	if (aspectRatio > 0.0f) {
		aspectRatio_ = aspectRatio;
	}
}

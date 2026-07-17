#pragma once

#include "Matrix4x4.h"
#include "Vector.h"

// カメラのTransform、View行列、Projection行列、マウス操作をまとめて管理するクラス
class Camera {
public:
	///// ----- 初期化処理 ----- /////
	// 描画領域の縦横比を受け取り、カメラを初期位置へ設定する
	void Initialize(float aspectRatio);

	///// ----- 更新処理 ----- /////
	// TransformからView行列とProjection行列を作り直す
	void Update();

	///// ----- マウス操作 ----- /////
	// Scene上の左ドラッグ量からカメラの向きを変更する
	void RotateByMouse(const Vector2& mouseDelta);
	// Scene上の右ドラッグ量からカメラの位置を平行移動する
	void MoveByMouse(const Vector2& mouseDelta);
	// Scene上のホイール量からカメラを前後移動する
	void ZoomByMouse(float wheel);
	// ウィンドウサイズ変更後の縦横比をProjection行列へ反映する
	void SetAspectRatio(float aspectRatio);

	///// ----- リセット ----- /////
	// 回転と移動を初期カメラ位置へ戻す
	void Reset();

	///// ----- Getter ----- /////
	// ImGuiからカメラのScale、Rotate、Translateを操作する
	Transform& GetTransform();
	const Transform& GetTransform() const;

	// 3DオブジェクトのWVP計算へ渡す行列を取得する
	const Matrix4x4& GetViewMatrix() const;
	const Matrix4x4& GetProjectionMatrix() const;

private:
	// カメラ自身の拡縮、回転、位置
	Transform transform_{};

	// Updateで計算し、各3Dオブジェクトが参照する行列
	Matrix4x4 viewMatrix_{};
	Matrix4x4 projectionMatrix_{};

	// 透視投影行列を作成するための設定値
	float aspectRatio_ = 1.0f;
	float fovY_ = 0.45f;
	float nearClip_ = 0.1f;
	float farClip_ = 100.0f;

	// マウスの入力値へ掛ける操作速度
	float rotateSpeed_ = 0.005f;
	float moveSpeed_ = 0.01f;
	float zoomSpeed_ = 0.3f;
};


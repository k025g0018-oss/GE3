#include "Object3D.h"

Object3D::Object3D() {
	// 起動時の表示モードを設定してから、全パラメーターを初期化する
	displayMode_ = 1;
	Reset();
}

void Object3D::Update() {
	if (isPlaying_) {
		transform_.rotate.y += 0.01f;
	}
}

void Object3D::Reset() {
	transform_ = {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
	isPlaying_ = false;
	// displayMode_は変更せず、リセット前に選択されていたモードを維持する

	// 既存の三角形・三角錐の初期配置を維持する
	for (int i = 0; i < 3; ++i) {
		triangle1Scale_[i] = 1.0f;
		triangle1Rotate_[i] = 0.0f;
		triangle1Translate_[i] = 0.0f;
		triangle2Scale_[i] = 1.0f;
		triangle2Rotate_[i] = 0.0f;
		triangle2Translate_[i] = 0.0f;
		pyramid1Scale_[i] = 1.0f;
		pyramid1Rotate_[i] = 0.0f;
		pyramid1Translate_[i] = 0.0f;
		pyramid2Scale_[i] = 1.0f;
		pyramid2Rotate_[i] = 0.0f;
		pyramid2Translate_[i] = 0.0f;
	}
	// 三角形と三角錐を、プログラム起動時と同じ初期位置へ戻す
	pyramid1Translate_[0] = -0.3f;
	pyramid2Translate_[0] = 0.3f;
}

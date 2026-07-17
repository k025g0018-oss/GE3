#pragma once

#include "Vector.h"

// 3Dオブジェクトの状態をmain.cppから分離して管理するクラス
class Object3D {
public:
	Object3D();

	// Start中だけ回転を進める
	void Update();
	// Transformと各頂点設定を初期状態へ戻す
	void Reset();

	Transform& GetTransform() { return transform_; }
	const Transform& GetTransform() const { return transform_; }
	bool& GetIsPlaying() { return isPlaying_; }
	bool IsPlaying() const { return isPlaying_; }
	int& GetDisplayMode() { return displayMode_; }
	int GetDisplayMode() const { return displayMode_; }

	float* GetTriangle1Scale() { return triangle1Scale_; }
	float* GetTriangle1Rotate() { return triangle1Rotate_; }
	float* GetTriangle1Translate() { return triangle1Translate_; }
	float* GetTriangle2Scale() { return triangle2Scale_; }
	float* GetTriangle2Rotate() { return triangle2Rotate_; }
	float* GetTriangle2Translate() { return triangle2Translate_; }
	float* GetPyramid1Scale() { return pyramid1Scale_; }
	float* GetPyramid1Rotate() { return pyramid1Rotate_; }
	float* GetPyramid1Translate() { return pyramid1Translate_; }
	float* GetPyramid2Scale() { return pyramid2Scale_; }
	float* GetPyramid2Rotate() { return pyramid2Rotate_; }
	float* GetPyramid2Translate() { return pyramid2Translate_; }

private:
	Transform transform_{};
	bool isPlaying_ = false;
	int displayMode_ = 0;
	float triangle1Scale_[3]{};
	float triangle1Rotate_[3]{};
	float triangle1Translate_[3]{};
	float triangle2Scale_[3]{};
	float triangle2Rotate_[3]{};
	float triangle2Translate_[3]{};
	float pyramid1Scale_[3]{};
	float pyramid1Rotate_[3]{};
	float pyramid1Translate_[3]{};
	float pyramid2Scale_[3]{};
	float pyramid2Rotate_[3]{};
	float pyramid2Translate_[3]{};
};

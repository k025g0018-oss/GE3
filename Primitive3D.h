#pragma once

#include "Matrix4x4.h"
#include "VertexBuffer.h"
#include "TransformationMatrix.h"
#include "Material.h"

#include <d3d12.h>
#include <cstdint>

// 三角形・三角錐の頂点、Material、WVP、描画をまとめて管理するクラス
class Primitive3D {
public:
	Primitive3D() = default;
	~Primitive3D();

	// DirectX12リソースの二重解放を防ぐため、コピーは禁止する
	Primitive3D(const Primitive3D&) = delete;
	Primitive3D& operator=(const Primitive3D&) = delete;

	///// ----- 初期化・更新・描画 ----- /////
	// 頂点、Material、WVPの各リソースを作成する
	void Initialize(ID3D12Device* device);
	// 再生中の回転、頂点データ、WVP行列を更新する
	void Update(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix);
	// 現在のモードに必要な頂点だけを描画する
	void Draw(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle) const;

	///// ----- リセット・解放 ----- /////
	// 現在のモードを維持したままTransformを初期値へ戻す
	void Reset();
	// Primitive3Dが所有しているDirectX12リソースを解放する
	void Finalize();

	///// ----- 共通設定 ----- /////
	// Propertiesからオブジェクト全体のTransformを操作する
	Transform& GetTransform() {
		return transform_;
	}
	const Transform& GetTransform() const {
		return transform_;
	}
	// PropertiesからMaterialの色を操作する
	Vector4& GetColor() {
		return materialData_->color;
	}
	// PropertiesからPrimitive3Dのライティング状態を取得する
	bool IsLightingEnabled() const {
		return materialData_->enableLighting != 0;
	}
	// 現在選択している表示モードのライティングを切り替える
	void SetLightingEnabled(bool enabled) {
		materialData_->enableLighting = enabled ? 1 : 0;
	}
	// StartとStopから自動回転の状態を操作する
	bool& GetIsPlaying() {
		return isPlaying_;
	}
	bool IsPlaying() const {
		return isPlaying_;
	}
	// 0:なし、1:三角形1枚、2:三角形2枚、3:三角錐1個、4:三角錐2個、5:ParticleSystem
	int& GetDisplayMode() {
		return displayMode_;
	}
	int GetDisplayMode() const {
		return displayMode_;
	}

	///// ----- モードごとの個別Transform ----- /////
	// 三角形を2枚表示するモードで、それぞれを個別に操作する
	float* GetTriangle1Scale() {
		return triangle1Scale_;
	}
	float* GetTriangle1Rotate() {
		return triangle1Rotate_;
	}
	float* GetTriangle1Translate() {
		return triangle1Translate_;
	}
	float* GetTriangle2Scale() {
		return triangle2Scale_;
	}
	float* GetTriangle2Rotate() {
		return triangle2Rotate_;
	}
	float* GetTriangle2Translate() {
		return triangle2Translate_;
	}
	// 三角錐を2個表示するモードで、それぞれを個別に操作する
	float* GetPyramid1Scale() {
		return pyramid1Scale_;
	}
	float* GetPyramid1Rotate() {
		return pyramid1Rotate_;
	}
	float* GetPyramid1Translate() {
		return pyramid1Translate_;
	}
	float* GetPyramid2Scale() {
		return pyramid2Scale_;
	}
	float* GetPyramid2Rotate() {
		return pyramid2Rotate_;
	}
	float* GetPyramid2Translate() {
		return pyramid2Translate_;
	}

private:
	// displayMode_に合わせてVertexBufferへ書き込む頂点を作成する
	void GenerateVertices();
	// 2個表示する図形へ個別の拡縮・回転・移動を適用する
	VertexData ApplyLocalTransform(const VertexData& vertex, const float* scale, const float* rotate, const float* translate) const;

	// 最大構成は三角錐2個なので、12頂点×2個分を確保する
	static constexpr uint32_t kMaxVertexCount = 24;
	// Primitive3D専用の頂点リソース
	VertexBuffer vertexBuffer_;
	// ピクセルシェーダーへ渡すMaterialの色
	ID3D12Resource* materialResource_ = nullptr;
	// Primitive3D専用Material
	Material* materialData_ = nullptr;
	// 頂点シェーダーへ渡すWorld・View・Projection行列
	ID3D12Resource* wvpResource_ = nullptr;
	TransformationMatrix* wvpData_ = nullptr;

	// 図形全体へ適用するTransformと再生状態
	Transform transform_{};
	bool isPlaying_ = false;
	// 現在表示するモードと、DrawInstancedへ渡す頂点数
	int displayMode_ = 0;
	uint32_t drawVertexCount_ = 0;

	// 三角形2枚を個別に動かすためのTransform
	float triangle1Scale_[3]{};
	float triangle1Rotate_[3]{};
	float triangle1Translate_[3]{};
	float triangle2Scale_[3]{};
	float triangle2Rotate_[3]{};
	float triangle2Translate_[3]{};
	// 三角錐2個を個別に動かすためのTransform
	float pyramid1Scale_[3]{};
	float pyramid1Rotate_[3]{};
	float pyramid1Translate_[3]{};
	float pyramid2Scale_[3]{};
	float pyramid2Rotate_[3]{};
	float pyramid2Translate_[3]{};
};

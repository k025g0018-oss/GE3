#pragma once

#include "Material.h"
#include "Matrix4x4.h"
#include "ModelLoader.h"
#include "TransformationMatrix.h"
#include "VertexBuffer.h"

#include <d3d12.h>
#include <string>

///// ----- Model ----- /////

// OBJモデルのリソースと描画処理を管理する
class Model {
public:
	Model() = default;
	~Model();

	Model(const Model&) = delete;
	Model& operator=(const Model&) = delete;

	/// --- 初期化 ---
	// OBJファイルを読み込み、描画に必要なリソースを作る
	void Initialize(
		ID3D12Device* device,
		const std::string& directoryPath,
		const std::string& filename
	);

	/// --- 更新 ---
	// TransformからWorld行列とWVP行列を作る
	void Update(
		const Matrix4x4& viewMatrix,
		const Matrix4x4& projectionMatrix
	);

	/// --- 描画 ---
	// 読み込んだOBJモデルを描画する
	void Draw(
		ID3D12GraphicsCommandList* commandList,
		D3D12_GPU_DESCRIPTOR_HANDLE textureHandle
	) const;

	/// --- 終了処理 ---
	// Modelが所有しているリソースを解放する
	void Finalize();

	/// --- 取得 ---
	// ImGuiなどからモデルの位置・回転・拡縮を変更する
	Transform& GetTransform();

	// 読み込んだモデルの頂点数を取得する
	uint32_t GetVertexCount() const;

	// モデルのライティング方式を取得する
	int32_t& GetLightingMode() {
		return materialData_->lightingMode;
	}

	// MTLで指定されたテクスチャのファイルパスを取得する
	const std::string& GetTextureFilePath() const {
		return modelData_.material.textureFilePath;
	}

	// モデルのライティングが有効か取得する
	bool IsLightingEnabled() const {
		return materialData_->enableLighting != 0;
	}

	// モデルのライティングを切り替える
	void SetLightingEnabled(bool enabled) {
		materialData_->enableLighting = enabled ? 1 : 0;
	}

private:
	// OBJから読み込んだモデルデータ
	ModelData modelData_;

	// OBJモデル専用の頂点バッファ
	VertexBuffer vertexBuffer_;

	// OBJモデル専用のマテリアル
	ID3D12Resource* materialResource_ = nullptr;
	Material* materialData_ = nullptr;

	// OBJモデル専用のWVP
	ID3D12Resource* wvpResource_ = nullptr;
	TransformationMatrix* wvpData_ = nullptr;

	// モデルの拡縮・回転・移動
	Transform transform_{};
};


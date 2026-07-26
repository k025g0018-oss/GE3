#pragma once
#include "Matrix4x4.h"
#include "VertexBuffer.h"
#include "TransformationMatrix.h"
#include "Material.h"

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

class Sphere {
public:
	Sphere() = default;
	~Sphere();

	Sphere(const Sphere&) = delete;
	Sphere& operator=(const Sphere&) = delete;

	///// ----- 初期化処理 ----- /////
	void Initialize(
		ID3D12Device* device,
		uint32_t maxSubdivision
	);

	///// ----- 更新処理 ----- /////
	void Update(
		const Matrix4x4& viewMatrix,
		const Matrix4x4& projectionMatrix
	);

	///// ----- 描画処理 ----- /////
	void Draw(
		ID3D12GraphicsCommandList* commandList,
		D3D12_GPU_DESCRIPTOR_HANDLE textureHandle
	) const;

	/// <summary>
	/// セッター
	/// </summary>
	/// <param name="subdivision"></param>
	void SetSubdivision(uint32_t subdivision);

	void Finalize();

	/// <summary>
	/// ゲッター
	/// </summary>
	/// <returns></returns>
	Transform& GetTransform();
	uint32_t GetSubdivision() const;
	uint32_t GetVertexCount() const;
	// ImGuiからライティング方式を直接変更する
	int32_t& GetLightingMode() {
		return materialData_->lightingMode;
	}

	// Sphereのライティング設定を取得する
	bool IsLightingEnabled() const {
		return materialData_->enableLighting != 0;
	}

	// ImGuiからSphereの色を変更する
	Vector4& GetColor() {
		return materialData_->color;
	}

	// Sphereのライティングを切り替える
	void SetLightingEnabled(bool enabled) {
		materialData_->enableLighting = enabled ? 1 : 0;
	}

private:
	// 分割数が変更されたときに頂点を作り直す
	void GenerateVertices();

	// Sphereの頂点番号を保存するIndex Resource
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

	// Index Resourceへ書き込むためのアドレス
	uint32_t* indexData_ = nullptr;

	// 描画時にIndex Resourceの情報を渡すView
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

	// 現在の分割数で描画するインデックス数
	uint32_t indexCount_ = 0;

	// Sphere専用VertexBuffer
	VertexBuffer vertexBuffer_;

	// Sphere専用Material
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	// Sphere専用のMaterialを書き込む
	Material* materialData_ = nullptr;

	// Sphere専用WVP
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
	TransformationMatrix* wvpData_ = nullptr;

	Transform transform_{};
	uint32_t subdivision_ = 16;
	uint32_t maxSubdivision_ = 32;
	uint32_t vertexCount_ = 0;

};

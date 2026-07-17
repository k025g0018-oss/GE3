#pragma once
#include "Matrix4x4.h"
#include "VertexBuffer.h"

#include <d3d12.h>
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

private:
	// 分割数が変更されたときに頂点を作り直す
	void GenerateVertices();

	// Sphere専用VertexBuffer
	VertexBuffer vertexBuffer_;

	// Sphere専用Material
	ID3D12Resource* materialResource_ = nullptr;
	Vector4* materialData_ = nullptr;

	// Sphere専用WVP
	ID3D12Resource* wvpResource_ = nullptr;
	Matrix4x4* wvpData_ = nullptr;

	Transform transform_{};
	uint32_t subdivision_ = 16;
	uint32_t maxSubdivision_ = 32;
	uint32_t vertexCount_ = 0;

};
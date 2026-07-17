#pragma once

#include "Matrix4x4.h"
#include "VertexBuffer.h"

#include <d3d12.h>
#include <cstdint>

///// ----- Sprite ----- /////

class Sprite {
public:
	Sprite() = default;
	~Sprite();

	Sprite(const Sprite&) = delete;
	Sprite& operator=(const Sprite&) = delete;

	/// --- 初期化 ---
	// Sprite用の頂点、Material、WVPを作成
	void Initialize(ID3D12Device* device, uint32_t clientWidth, uint32_t clientHeight, float spriteWidth, float spriteHeight);

	/// --- 更新 ---
	// SRTと正射影行列からWVPを更新
	void Update();

	/// --- 描画 ---
	// Sprite用のリソースを設定して2つの三角形を描画
	void Draw(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle) const;

	/// --- 取得 ---
	// ImGuiから変更するTransformを取得
	Transform& GetTransform() { return transform_; }

	// ImGuiから変更する色を取得
	Vector4& GetColor() { return *materialData_; }

	/// --- リセット ---
	// SRTと色を初期値へ戻す
	void Reset();

	/// --- 終了処理 ---
	// Spriteが所有するリソースを解放
	void Finalize();

private:
	static constexpr uint32_t kVertexCount = 6;

	VertexBuffer vertexBuffer_;
	ID3D12Resource* materialResource_ = nullptr;
	Vector4* materialData_ = nullptr;
	ID3D12Resource* wvpResource_ = nullptr;
	Matrix4x4* wvpData_ = nullptr;
	Transform transform_{};
	uint32_t clientWidth_ = 0;
	uint32_t clientHeight_ = 0;
};

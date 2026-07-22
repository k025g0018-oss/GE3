#pragma once

#include "DescriptorHeap.h"

#include "externals/DirectXTex/DirectXTex.h"

#include <cstdint>
#include <vector>

///// ----- SRV(Shader Resource View) ----- /////

class ShaderResourceView {
public:
	/// --- 初期化 ---
	// SRVを配置するDescriptorHeapの位置を設定
	void Initialize(const DescriptorHeap& descriptorHeap, uint32_t firstDescriptorIndex, uint32_t srvCount);

	/// --- Texture用SRV ---
	// TextureをShaderから参照するためのSRVを作成
	void CreateTextureSRV(ID3D12Device* device, uint32_t srvIndex, ID3D12Resource* textureResource, const DirectX::TexMetadata& metadata);

	/// --- 取得 ---
	// 描画時に使用するGPUハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t srvIndex) const;

private:
	// DescriptorHandleの計算をDescriptorHeapへ任せるため参照を保存する
	const DescriptorHeap* descriptorHeap_ = nullptr;

	// ImGuiが0番を使うため、Texture用のSRVの開始位置を保存する
	uint32_t firstDescriptorIndex_ = 0;

	std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> textureSrvHandlesGPU_;
};

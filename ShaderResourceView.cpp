#include "ShaderResourceView.h"

#include <cassert>

///// ----- SRV(Shader Resource View) ----- /////

/// --- 初期化 ---
// SRVを配置するDescriptorHeapの位置を設定
void ShaderResourceView::Initialize(const DescriptorHeap& descriptorHeap, uint32_t firstDescriptorIndex, uint32_t srvCount) {
	// Texture用SRVがHeapの範囲内に収まることを確認する
	assert(firstDescriptorIndex + srvCount <= descriptorHeap.GetDescriptorCount());

	descriptorHeap_ = &descriptorHeap;
	firstDescriptorIndex_ = firstDescriptorIndex;

	textureSrvHandlesGPU_.resize(srvCount);
}

/// --- Texture用SRV ---
// TextureをShaderから参照するためのSRVを作成
void ShaderResourceView::CreateTextureSRV(ID3D12Device* device, uint32_t srvIndex, ID3D12Resource* textureResource, const DirectX::TexMetadata& metadata) {

	assert(srvIndex < textureSrvHandlesGPU_.size());

	// metadataをもとにSRVの設定を行う
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata.mipLevels);

	// Texture番号をDescriptorHeap全体の番号へ変換する
	const uint32_t descriptorIndex =
		firstDescriptorIndex_ + srvIndex;

	// Handleの位置計算はDescriptorHeapクラスへ任せる
	const D3D12_CPU_DESCRIPTOR_HANDLE handleCPU =
		descriptorHeap_->GetCPUHandle(descriptorIndex);

	const D3D12_GPU_DESCRIPTOR_HANDLE handleGPU =
		descriptorHeap_->GetGPUHandle(descriptorIndex);

	// 計算したDescriptor位置へSRVを作成する
	device->CreateShaderResourceView(
		textureResource,
		&srvDesc,
		handleCPU
	);

	// 描画時に使うGPU HandleをTexture番号ごとに保存する
	textureSrvHandlesGPU_[srvIndex] = handleGPU;
}

/// --- 取得 ---
// 描画時に使用するGPUハンドルを取得
D3D12_GPU_DESCRIPTOR_HANDLE ShaderResourceView::GetGPUHandle(uint32_t srvIndex) const {
	assert(srvIndex < textureSrvHandlesGPU_.size());
	return textureSrvHandlesGPU_[srvIndex];
}

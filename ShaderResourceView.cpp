#include "ShaderResourceView.h"

#include <cassert>

///// ----- SRV(Shader Resource View) ----- /////

/// --- 初期化 ---
// SRVを配置するDescriptorHeapの位置を設定
void ShaderResourceView::Initialize(const DescriptorHeap& descriptorHeap, uint32_t firstDescriptorIndex, uint32_t srvCount) {
	// SRV1個分のサイズを取得する
	descriptorSize_ = descriptorHeap.GetDescriptorSize();

	// SRVヒープの先頭を取得する
	srvHandleCPU_ = descriptorHeap.GetCPUHandleStart();
	srvHandleGPU_ = descriptorHeap.GetGPUHandleStart();

	// ImGuiが0番を使うため、テクスチャは1番から使用する
	srvHandleCPU_.ptr += static_cast<SIZE_T>(descriptorSize_) * firstDescriptorIndex;
	srvHandleGPU_.ptr += static_cast<UINT64>(descriptorSize_) * firstDescriptorIndex;

	// 描画時に使用するGPUハンドルを保存する領域を作る
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

	// 使用するSRVの位置を計算する
	D3D12_CPU_DESCRIPTOR_HANDLE currentSrvHandleCPU = srvHandleCPU_;
	D3D12_GPU_DESCRIPTOR_HANDLE currentSrvHandleGPU = srvHandleGPU_;
	currentSrvHandleCPU.ptr += static_cast<SIZE_T>(descriptorSize_) * srvIndex;
	currentSrvHandleGPU.ptr += static_cast<UINT64>(descriptorSize_) * srvIndex;

	// SRVを生成する
	device->CreateShaderResourceView(textureResource, &srvDesc, currentSrvHandleCPU);

	// 描画時に使えるように保存する
	textureSrvHandlesGPU_[srvIndex] = currentSrvHandleGPU;
}

/// --- 取得 ---
// 描画時に使用するGPUハンドルを取得
D3D12_GPU_DESCRIPTOR_HANDLE ShaderResourceView::GetGPUHandle(uint32_t srvIndex) const {
	assert(srvIndex < textureSrvHandlesGPU_.size());
	return textureSrvHandlesGPU_[srvIndex];
}

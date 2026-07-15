#pragma once

#include "DescriptorHeap.h"

#include <d3d12.h>
#include <cstdint>

///// ----- DSV(Depth Stencil View) ----- /////

class DepthStencilView {
public:
	DepthStencilView() = default;
	~DepthStencilView();

	DepthStencilView(const DepthStencilView&) = delete;
	DepthStencilView& operator=(const DepthStencilView&) = delete;

	/// --- 初期化 ---
	// DepthStencilTextureとDSVを作成
	void Initialize(ID3D12Device* device, const DescriptorHeap& dsvDescriptorHeap, int32_t width, int32_t height);

	/// --- 取得 ---
	// DSVのCPUハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE GetHandle() const { return dsvHandle_; }

	/// --- 終了処理 ---
	// DepthStencilTextureを解放
	void Finalize();

private:
	/// --- DepthStencilTexture ---
	// DepthStencilTextureResourceを作成
	ID3D12Resource* CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height);

	ID3D12Resource* depthStencilResource_ = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};
};

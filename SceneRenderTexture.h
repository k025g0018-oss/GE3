#pragma once

#include "DescriptorHeap.h"

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

///// ----- SceneRenderTexture ----- /////

// ゲーム画面をImGuiのSceneパネルへ表示するための描画用テクスチャ
class SceneRenderTexture {
public:
	SceneRenderTexture() = default;
	~SceneRenderTexture();

	SceneRenderTexture(const SceneRenderTexture&) = delete;
	SceneRenderTexture& operator=(const SceneRenderTexture&) = delete;

	// Scene用テクスチャ、RTV、SRVを作成する
	void Initialize(
		ID3D12Device* device,
		const DescriptorHeap& srvDescriptorHeap,
		uint32_t srvDescriptorIndex,
		uint32_t width,
		uint32_t height,
		DXGI_FORMAT format
	);

	// ゲーム描画前にRenderTarget状態へ切り替える
	void TransitionToRenderTarget(ID3D12GraphicsCommandList* commandList);

	// ImGui表示前にShaderResource状態へ切り替える
	void TransitionToShaderResource(ID3D12GraphicsCommandList* commandList);

	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return rtvHandle_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVHandle() const { return srvHandleGPU_; }

	// Scene用リソースを解放する
	void Finalize();

private:
	void Transition(
		ID3D12GraphicsCommandList* commandList,
		D3D12_RESOURCE_STATES nextState
	);

	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_{};
	D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
};

#include "SceneRenderTexture.h"

#include <cassert>

///// ----- SceneRenderTexture ----- /////

SceneRenderTexture::~SceneRenderTexture() {
	Finalize();
}

// Scene用テクスチャ、RTV、SRVを作成する
void SceneRenderTexture::Initialize(
	ID3D12Device* device,
	const DescriptorHeap& srvDescriptorHeap,
	uint32_t srvDescriptorIndex,
	uint32_t width,
	uint32_t height,
	DXGI_FORMAT format
) {
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = format;
	clearValue.Color[0] = 0.1f;
	clearValue.Color[1] = 0.25f;
	clearValue.Color[2] = 0.5f;
	clearValue.Color[3] = 1.0f;

	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		currentState_,
		&clearValue,
		IID_PPV_ARGS(&resource_)
	);
	assert(SUCCEEDED(hr));

	// Scene専用RTVは既存のSwapChain用RTVと分けて管理する
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.NumDescriptors = 1;
	hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap_));
	assert(SUCCEEDED(hr));

	rtvHandle_ = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	device->CreateRenderTargetView(resource_, nullptr, rtvHandle_);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	const D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU =
		srvDescriptorHeap.GetCPUHandle(srvDescriptorIndex);
	srvHandleGPU_ = srvDescriptorHeap.GetGPUHandle(srvDescriptorIndex);
	device->CreateShaderResourceView(resource_, &srvDesc, srvHandleCPU);
}

// ゲーム描画前にRenderTarget状態へ切り替える
void SceneRenderTexture::TransitionToRenderTarget(ID3D12GraphicsCommandList* commandList) {
	Transition(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

// ImGui表示前にShaderResource状態へ切り替える
void SceneRenderTexture::TransitionToShaderResource(ID3D12GraphicsCommandList* commandList) {
	Transition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void SceneRenderTexture::Transition(
	ID3D12GraphicsCommandList* commandList,
	D3D12_RESOURCE_STATES nextState
) {
	if (currentState_ == nextState) {
		return;
	}

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource_;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = currentState_;
	barrier.Transition.StateAfter = nextState;
	commandList->ResourceBarrier(1, &barrier);
	currentState_ = nextState;
}

// Scene用リソースを解放する
void SceneRenderTexture::Finalize() {
	if (resource_ != nullptr) {
		resource_->Release();
		resource_ = nullptr;
	}
	if (rtvDescriptorHeap_ != nullptr) {
		rtvDescriptorHeap_->Release();
		rtvDescriptorHeap_ = nullptr;
	}
	rtvHandle_ = {};
	srvHandleGPU_ = {};
	currentState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

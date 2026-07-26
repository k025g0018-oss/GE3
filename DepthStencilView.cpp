#include "DepthStencilView.h"

#include <cassert>

///// ----- DSV(Depth Stencil View) ----- /////

DepthStencilView::~DepthStencilView() {
	Finalize();
}

/// --- 初期化 ---
// DepthStencilTextureとDSVを作成
void DepthStencilView::Initialize(ID3D12Device* device, const DescriptorHeap& dsvDescriptorHeap, int32_t width, int32_t height) {
	// 深度ステンシルテクスチャリソースを作る
	// 生成したResourceの所有権をResourceObjectへ渡す
	depthStencilResource_.Reset(
		CreateDepthStencilTextureResource(device, width, height)
	);

	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // Format、基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2dTexture
	// DSVHeapの先頭にDSVを作る
	dsvHandle_ = dsvDescriptorHeap.GetCPUHandleStart();

	// DirectXの関数へ渡すときは生ポインタを取得する
	device->CreateDepthStencilView(
		depthStencilResource_.Get(),
		&dsvDesc,
		dsvHandle_
	);
}

/// --- DepthStencilTexture ---
// DepthStencilTexture
ID3D12Resource* DepthStencilView::CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height) {
	// 生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width; // Textureの幅
	resourceDesc.Height = height; // Textureの高さ
	resourceDesc.MipLevels = 1; // mipmapの数
	resourceDesc.DepthOrArraySize = 1; // 奥行きor配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使う通知

	// 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f; // 1.0f(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマット。Resourceに合わせる

	// Resourceの生成
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込む状態にしておく
		&depthClearValue, // Clear最適値
		IID_PPV_ARGS(&resource)
	);
	assert(SUCCEEDED(hr));

	return resource;
}

/// --- 終了処理 ---
// DepthStencilTextureを解放
void DepthStencilView::Finalize() {
	dsvHandle_ = {};
	
	// Deviceを解放する前にDepthStencilResourceを解放する
	// デストラクタから再度Resetされてもnullptrなので二重解放されない
	depthStencilResource_.Reset();
}

#include "DescriptorHeap.h"

#include <cassert>

///// ----- DescriptorHeap ----- /////

DescriptorHeap::~DescriptorHeap() {
	Finalize();
}

/// --- 初期化 ---
// ID3D12DescriptorHeapの作成
void DescriptorHeap::Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
	// ディスクリプタヒープ生成
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType; // ヒープタイプ
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap_));
	assert(SUCCEEDED(hr));

	// Descriptor1個分のサイズを保存
	descriptorSize_ = device->GetDescriptorHandleIncrementSize(heapType);
}

/// --- 取得 ---
// CPU側の先頭ハンドルを取得
D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCPUHandleStart() const {
	return descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
}

// GPU側の先頭ハンドルを取得
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGPUHandleStart() const {
	return descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
}

/// --- 終了処理 ---
// DescriptorHeapを解放
void DescriptorHeap::Finalize() {
	if (descriptorHeap_ != nullptr) {
		descriptorHeap_->Release();
		descriptorHeap_ = nullptr;
	}
	descriptorSize_ = 0;
}

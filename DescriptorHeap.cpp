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
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(descriptorHeap_.GetAddressOf()));
	assert(SUCCEEDED(hr));

	// Descriptor1個分のサイズを保存
	descriptorSize_ = device->GetDescriptorHandleIncrementSize(heapType);
	descriptorCount_ = numDescriptors;
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

// 指定した番号のCPUディスクリプタハンドルを取得する
D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCPUHandle(uint32_t index) const {
	assert(index < descriptorCount_);

	// Heapの先頭から、指定したインデックスまで移動する
	D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUHandleStart();
	// CPU HandleのptrはSIZE_Tなので、計算結果もSIZE_Tへ合わせる
	// handle.ptr += descriptorSize_ * index;
	handle.ptr += static_cast<SIZE_T>(descriptorSize_) * index;

	return handle;
}

// 指定した番号のGPUディスクリプタハンドルを取得する
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGPUHandle(uint32_t index) const {
	assert(index < descriptorCount_);

	// Heapの先頭から、指定したインデックスまで移動する
	D3D12_GPU_DESCRIPTOR_HANDLE handle = GetGPUHandleStart();
	// GPU HandleのptrはUINT64なので、計算結果もUINT64へ合わせる
	// handle.ptr += descriptorSize_ * index;
	handle.ptr += static_cast<UINT64>(descriptorSize_) * index;

	return handle;
}

/// --- 終了処理 ---
// DescriptorHeapを解放
void DescriptorHeap::Finalize() {
	// DescriptorHeapはComPtrのデストラクタが自動解放する
	descriptorSize_ = 0;
	descriptorCount_ = 0;
}

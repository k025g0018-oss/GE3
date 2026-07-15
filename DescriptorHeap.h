#pragma once

#include <d3d12.h>

///// ----- DescriptorHeap ----- /////

class DescriptorHeap {
public:
	DescriptorHeap() = default;
	~DescriptorHeap();

	DescriptorHeap(const DescriptorHeap&) = delete;
	DescriptorHeap& operator=(const DescriptorHeap&) = delete;

	/// --- 初期化 ---
	// DescriptorHeapを作成
	void Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	/// --- 取得 ---
	// DescriptorHeapを取得
	ID3D12DescriptorHeap* Get() const { return descriptorHeap_; }

	// CPU側の先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandleStart() const;

	// GPU側の先頭ハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandleStart() const;

	// Descriptor1個分のサイズを取得
	UINT GetDescriptorSize() const { return descriptorSize_; }

	/// --- 終了処理 ---
	// DescriptorHeapを解放
	void Finalize();

private:
	ID3D12DescriptorHeap* descriptorHeap_ = nullptr;
	UINT descriptorSize_ = 0;
};

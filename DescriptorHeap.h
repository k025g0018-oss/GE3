#pragma once

#include <d3d12.h>
#include <cstdint>

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

	// 指定したインデックスのCPUディスクリプタハンドルを取得する
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t index) const;

	// 指定したインデックスのGPUディスクリプタハンドルを取得する
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t index) const;

	// Descriptor1個分のサイズを取得
	UINT GetDescriptorSize() const { return descriptorSize_; }

	// ヒープに確保したディスクリプタ数を取得する
	UINT GetDescriptorCount() const { return descriptorCount_; }

	/// --- 終了処理 ---
	// DescriptorHeapを解放
	void Finalize();

private:
	ID3D12DescriptorHeap* descriptorHeap_ = nullptr;
	UINT descriptorSize_ = 0;
	UINT descriptorCount_ = 0;
};

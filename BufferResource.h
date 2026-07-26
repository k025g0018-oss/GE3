#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstddef>

///// ----- BufferResource ----- /////

class BufferResource {
public:
	/// --- 生成 ---
	// UploadHeapのBufferResourceを作成
	// 生成したResourceの所有権をComPtrで返す
	static Microsoft::WRL::ComPtr<ID3D12Resource> Create(ID3D12Device* device, size_t sizeInBytes);
};

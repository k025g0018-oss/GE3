#pragma once

#include <d3d12.h>
#include <cstddef>

///// ----- BufferResource ----- /////

class BufferResource {
public:
	/// --- 生成 ---
	// UploadHeapのBufferResourceを作成
	static ID3D12Resource* Create(ID3D12Device* device, size_t sizeInBytes);
};

#pragma once

#include "VertexData.h"

#include <d3d12.h>
#include <cstdint>

///// ----- VertexBuffer ----- /////

class VertexBuffer {
public:
	VertexBuffer() = default;
	~VertexBuffer();

	VertexBuffer(const VertexBuffer&) = delete;
	VertexBuffer& operator=(const VertexBuffer&) = delete;

	/// --- 初期化 ---
	// 頂点データとVertexBufferViewを作成
	void Initialize(ID3D12Device* device, uint32_t maxVertexCount);

	/// --- 取得 ---
	// 頂点を書き込むアドレスを取得
	VertexData* GetData() const { return vertexData_; }

	// VertexBufferViewを取得
	const D3D12_VERTEX_BUFFER_VIEW& GetView() const { return vertexBufferView_; }

	/// --- 終了処理 ---
	// VertexBufferを解放
	void Finalize();

private:
	ID3D12Resource* vertexResource_ = nullptr;
	VertexData* vertexData_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
};

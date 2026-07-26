#include "VertexBuffer.h"

#include "BufferResource.h"

#include <cassert>

///// ----- VertexBuffer ----- /////

VertexBuffer::~VertexBuffer() {
	Finalize();
}

/// --- 初期化 ---
// 頂点データとVertexBufferViewを作成
void VertexBuffer::Initialize(ID3D12Device* device, uint32_t maxVertexCount) {
	/// --- VertexResource ---
	// VertexResourceを生成する
	vertexResource_ = BufferResource::Create(device, sizeof(VertexData) * maxVertexCount);

	// データを書き込むためのアドレスを取得
	HRESULT hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
	assert(SUCCEEDED(hr));

	/// --- VertexBufferView ---
	// 頂点バッファビューを作成する
	// リソースの先頭のアドレスから使う
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	// 使用するリソースのサイズは最大頂点数分のサイズにする
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * maxVertexCount;
	// 1頂点当たりのサイズ
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

/// --- 終了処理 ---
// VertexBufferを解放
void VertexBuffer::Finalize() {
	vertexData_ = nullptr;
	vertexBufferView_ = {};
	// VertexResourceはComPtrのデストラクタが自動解放する
}

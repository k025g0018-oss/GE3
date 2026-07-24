#include "Model.h"
#include "BufferResource.h"

#include <cassert>
#include <cstring>

///// ----- 解放処理 ----- /////
Model::~Model() {
	Finalize();
}

///// ----- 初期化処理 ----- /////
void Model::Initialize(
	ID3D12Device* device,
	const std::string& directoryPath,
	const std::string& filename
) {
	assert(device != nullptr);

	/// --- モデル読み込み ---

	// OBJファイルを読み込み、頂点データを取得する
	modelData_ =
		ModelLoader::LoadObjFile(
			directoryPath,
			filename
		);

	// OBJから頂点が読み込めていることを確認する
	assert(!modelData_.vertices.empty());

	/// --- 頂点バッファ ---

	// OBJモデルの頂点数に合わせて頂点バッファを作る
	const uint32_t vertexCount =
		static_cast<uint32_t>(modelData_.vertices.size());

	vertexBuffer_.Initialize(
		device,
		vertexCount
	);

	// 頂点バッファへ書き込むアドレスを取得する
	VertexData* vertexData =
		vertexBuffer_.GetData();

	assert(vertexData != nullptr);

	// OBJから読み込んだ頂点を頂点バッファへコピーする
	std::memcpy(
		vertexData,
		modelData_.vertices.data(),
		sizeof(VertexData) * modelData_.vertices.size()
	);

	/// --- Transform ---
	// モデルの拡縮・回転・移動を初期化する
	transform_ = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	/// --- Material ---
	// OBJモデル専用のMaterialリソースを作る
	materialResource_ =
		BufferResource::Create(
			device,
			sizeof(Material)
		);

	HRESULT hr = materialResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&materialData_)
	);

	assert(SUCCEEDED(hr));

	// モデルの色を白にする
	materialData_->color =
	{1.0f, 1.0f, 1.0f, 1.0f};

	// 最初はライティングを有効にする
	materialData_->enableLighting = true;

	// Lambertライティングを使う
	materialData_->lightingMode = 0;

	// UV変換を単位行列で初期化する
	materialData_->uvTransform =
		Matrix4x4::MakeIdentity4x4();

	/// --- WVP ---
	// OBJモデル専用のWVPリソースを作る
	wvpResource_ =
		BufferResource::Create(
			device,
			sizeof(TransformationMatrix)
		);

	hr = wvpResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&wvpData_)
	);

	assert(SUCCEEDED(hr));

	// WVP行列を単位行列で初期化する
	wvpData_->WVP =
		Matrix4x4::MakeIdentity4x4();

	// World行列を単位行列で初期化する
	wvpData_->World =
		Matrix4x4::MakeIdentity4x4();
}

///// ----- 更新処理 ----- /////
void Model::Update(
	const Matrix4x4& viewMatrix,
	const Matrix4x4& projectionMatrix
) {
	// TransformからWorld行列を作る
	const Matrix4x4 worldMatrix =
		Matrix4x4::MakeAffineMatrix(
			transform_.scale,
			transform_.rotate,
			transform_.translate
		);

	// World、View、Projectionを掛けてWVP行列を作る
	wvpData_->WVP =
		Matrix4x4::Multiply(
			worldMatrix,
			Matrix4x4::Multiply(
			viewMatrix,
			projectionMatrix
		)
		);

	// ライティングで使うWorld行列を設定する
	wvpData_->World = worldMatrix;
}

///// ----- 描画処理 ----- /////
void Model::Draw(
	ID3D12GraphicsCommandList* commandList,
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle
) const {
	assert(commandList != nullptr);
	assert(materialResource_ != nullptr);
	assert(wvpResource_ != nullptr);

	/// --- 頂点バッファ ---

	// OBJモデル専用の頂点バッファビューを取得する
	const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView =
		vertexBuffer_.GetView();

	// 描画に使用する頂点バッファを設定する
	commandList->IASetVertexBuffers(
		0,
		1,
		&vertexBufferView
	);

	// OBJモデルを三角形リストとして描画する
	commandList->IASetPrimitiveTopology(
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	);

	/// --- 描画リソース ---

	// マテリアルを設定する
	commandList->SetGraphicsRootConstantBufferView(
		0,
		materialResource_->GetGPUVirtualAddress()
	);

	// WVPを設定する
	commandList->SetGraphicsRootConstantBufferView(
		1,
		wvpResource_->GetGPUVirtualAddress()
	);

	// テクスチャを設定する
	commandList->SetGraphicsRootDescriptorTable(
		2,
		textureHandle
	);

	/// --- 描画 ---

	// OBJから読み込んだ頂点数を使って描画する
	commandList->DrawInstanced(
		static_cast<UINT>(modelData_.vertices.size()), 1, 0, 0);
}

///// ----- ImGuiなどからモデルの位置・回転・拡縮を変更する ----- /////
Transform& Model::GetTransform() {
	// ImGuiなどから変更できるようにTransformを返す
	return transform_;
}

///// ----- 読み込んだモデルの頂点数を取得する ----- /////
uint32_t Model::GetVertexCount() const {
	// OBJから読み込んだ頂点数を返す
	return static_cast<uint32_t>(
		modelData_.vertices.size()
		);
}

///// ----- 解放処理 ----- /////
void Model::Finalize() {
	// OBJモデル専用の頂点バッファを解放する
	vertexBuffer_.Finalize();

	materialData_ = nullptr;

	// Materialリソースを解放する
	if (materialResource_ != nullptr) {
		materialResource_->Release();
		materialResource_ = nullptr;
	}

	wvpData_ = nullptr;

	// WVPリソースを解放する
	if (wvpResource_ != nullptr) {
		wvpResource_->Release();
		wvpResource_ = nullptr;
	}

	// CPU側に保持している頂点データを解放する
	modelData_.vertices.clear();
}
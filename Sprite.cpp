#include "Sprite.h"

#include "BufferResource.h"

#include <cassert>

///// ----- Sprite ----- /////

Sprite2D::~Sprite2D() {
	Finalize();
}

/// --- 初期化 ---
// Sprite用の頂点、Material、WVPを作成
void Sprite2D::Initialize(ID3D12Device* device, uint32_t clientWidth, uint32_t clientHeight, float spriteWidth, float spriteHeight) {
	assert(device != nullptr);
	assert(clientWidth > 0);
	assert(clientHeight > 0);

	clientWidth_ = clientWidth;
	clientHeight_ = clientHeight;

	/// --- VertexResourceとVertexBufferView ---
	// 重複しない4頂点分のVertexBufferを作成する
	vertexBuffer_.Initialize(device, kVertexCount);
	VertexData* vertexData = vertexBuffer_.GetData();

	// 頂点0：左下
	vertexData[0] = {
		{0.0f, spriteHeight, 0.0f, 1.0f},
		{0.0f, 1.0f}
	};

	// 頂点1：左上
	vertexData[1] = {
		{0.0f, 0.0f, 0.0f, 1.0f},
		{0.0f, 0.0f}
	};

	// 頂点2：右下
	vertexData[2] = {
		{spriteWidth, spriteHeight, 0.0f, 1.0f},
		{1.0f, 1.0f}
	};

	// 頂点3：右上
	vertexData[3] = {
		{spriteWidth, 0.0f, 0.0f, 1.0f},
		{1.0f, 0.0f}
	};

	/// --- IndexResourceとIndexBufferView ---
	// 2つの三角形に必要な6インデックス分のResourceを作成する
	indexResource_ = BufferResource::Create(
		device,
		sizeof(uint32_t) * kIndexCount
	);

	// Index Resourceへ書き込むアドレスを取得する
	uint32_t* indexData = nullptr;
	HRESULT hr = indexResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&indexData)
	);
	assert(SUCCEEDED(hr));

	// 1つ目の三角形：左下、左上、右下
	indexData[0] = 0;
	indexData[1] = 1;
	indexData[2] = 2;

	// 2つ目の三角形：左上、右上、右下
	indexData[3] = 1;
	indexData[4] = 3;
	indexData[5] = 2;

	// Index Resourceの先頭アドレスを設定する
	indexBufferView_.BufferLocation =
		indexResource_->GetGPUVirtualAddress();

	// 使用するIndex Resource全体のサイズを設定する
	indexBufferView_.SizeInBytes =
		sizeof(uint32_t) * kIndexCount;

	// Indexデータはuint32_tとして扱う
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	/// --- MaterialResource ---
	// Sprite専用の色を保存するMaterialを作成
	materialResource_ = BufferResource::Create(device, sizeof(Material));
	hr = materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	assert(SUCCEEDED(hr));

	/// --- WVPResource ---
	// Sprite専用のWVPを保存するConstantBufferを作成
	wvpResource_ = BufferResource::Create(device, sizeof(TransformationMatrix));
	hr = wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
	assert(SUCCEEDED(hr));

	// Transform・色・ライティング設定を初期化する
	Reset();

	// 初期化したTransformからWVP行列を作成する
	Update();
}

/// --- 更新 ---
// SRTと正射影行列からWVPを更新
void Sprite2D::Update() {
	assert(wvpData_ != nullptr);

	// SpriteのSRTからWorld行列を作成
	const Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(
		transform_.scale,
		transform_.rotate,
		transform_.translate
	);

	// 2D描画ではCameraを使用しないため単位行列にする
	const Matrix4x4 viewMatrix = Matrix4x4::MakeIdentity4x4();

	// 左上を原点とする画面座標用の正射影行列を作成
	const Matrix4x4 projectionMatrix = Matrix4x4::MakeOrthographicMatrix(
		0.0f,
		0.0f,
		static_cast<float>(clientWidth_),
		static_cast<float>(clientHeight_),
		0.0f,
		100.0f
	);

	// ImGuiで設定されたSRTからUV Transform行列を作成する
	materialData_->uvTransform =
		Matrix4x4::MakeAffineMatrix(
			uvTransform_.scale,
			{0.0f, 0.0f, uvTransform_.rotate.z},
		{
			uvTransform_.translate.x,
			uvTransform_.translate.y,
			0.0f
		}
		);

	// 頂点の座標変換に使用するWVPを設定する
	wvpData_->WVP = Matrix4x4::Multiply(
		worldMatrix,
		Matrix4x4::Multiply(viewMatrix, projectionMatrix)
	);

	// 法線をワールド空間へ変換するため、Worldも送る
	wvpData_->World = worldMatrix;
}

/// --- 描画 ---
// Sprite用のリソースを設定して2つの三角形を描画
void Sprite2D::Draw(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle) const {
	assert(commandList != nullptr);

	// Sprite専用のVertexBufferViewを設定
	const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView = vertexBuffer_.GetView();
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

	// SpriteのIndexBufferViewを設定する
	commandList->IASetIndexBuffer(&indexBufferView_);

	// Sprite専用のMaterialとWVPを設定
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());

	// Sprite用に選択されたTextureを設定
	commandList->SetGraphicsRootDescriptorTable(2, textureHandle);

	// 6つのインデックスを使用して2つの三角形を描画する
	commandList->DrawIndexedInstanced(kIndexCount, 1, 0, 0, 0);
}

/// --- リセット ---
// SRTと色を初期値へ戻す
void Sprite2D::Reset() {
	transform_.scale = {1.0f, 1.0f, 1.0f};
	transform_.rotate = {0.0f, 0.0f, 0.0f};
	transform_.translate = {0.0f, 0.0f, 0.0f};

	// Spriteの色とライティング設定を初期値へ戻す
	if (materialData_ != nullptr) {
		// Spriteの色を白へ戻す
		materialData_->color = {1.0f, 1.0f, 1.0f, 1.0f};

		// SpriteにはLightingを適用しない
		materialData_->enableLighting = false;

		// 初期状態ではUV座標を変化させない
		materialData_->uvTransform =
			Matrix4x4::MakeIdentity4x4();
	}

	// UV Transformの操作値を初期状態へ戻す
	uvTransform_.scale = {1.0f, 1.0f, 1.0f};
	uvTransform_.rotate = {0.0f, 0.0f, 0.0f};
	uvTransform_.translate = {0.0f, 0.0f, 0.0f};
}

/// --- 終了処理 ---
// Spriteが所有するリソースを解放
void Sprite2D::Finalize() {
	vertexBuffer_.Finalize();

	materialData_ = nullptr;
	// MaterialResourceはComPtrのデストラクタが自動解放する

	wvpData_ = nullptr;
	// WVPResourceはComPtrのデストラクタが自動解放する

	clientWidth_ = 0;
	clientHeight_ = 0;

	// SpriteのIndex Resourceを解放する
	indexBufferView_ = {};
	// IndexResourceはComPtrのデストラクタが自動解放する
}

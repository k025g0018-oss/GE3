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
	// 四角形を構成する6頂点分のVertexBufferを作成
	vertexBuffer_.Initialize(device, kVertexCount);
	VertexData* vertexData = vertexBuffer_.GetData();

	// 1つ目の三角形
	vertexData[0] = {{0.0f, spriteHeight, 0.0f, 1.0f}, {0.0f, 1.0f}};
	vertexData[1] = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}};
	vertexData[2] = {{spriteWidth, spriteHeight, 0.0f, 1.0f}, {1.0f, 1.0f}};

	// 2つ目の三角形
	vertexData[3] = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}};
	vertexData[4] = {{spriteWidth, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}};
	vertexData[5] = {{spriteWidth, spriteHeight, 0.0f, 1.0f}, {1.0f, 1.0f}};

	/// --- MaterialResource ---
	// Sprite専用の色を保存するMaterialを作成
	materialResource_ = BufferResource::Create(device, sizeof(Material));
	HRESULT hr = materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
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

	// Sprite専用のMaterialとWVPを設定
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());

	// Sprite用に選択されたTextureを設定
	commandList->SetGraphicsRootDescriptorTable(2, textureHandle);

	// 6頂点で1枚のSpriteを描画
	commandList->DrawInstanced(kVertexCount, 1, 0, 0);
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
	}
}

/// --- 終了処理 ---
// Spriteが所有するリソースを解放
void Sprite2D::Finalize() {
	vertexBuffer_.Finalize();

	materialData_ = nullptr;
	if (materialResource_ != nullptr) {
		materialResource_->Release();
		materialResource_ = nullptr;
	}

	wvpData_ = nullptr;
	if (wvpResource_ != nullptr) {
		wvpResource_->Release();
		wvpResource_ = nullptr;
	}

	clientWidth_ = 0;
	clientHeight_ = 0;
}

#include "Sphere.h"
#include "BufferResource.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

/// <summary>
///  解放処理
/// </summary>
Sphere::~Sphere() {
	Finalize();
}

/// <summary>
/// 初期化処理
/// </summary>
/// <param name="device"></param>
/// <param name="maxSubdivision"></param>
void Sphere::Initialize(
	ID3D12Device* device,
	uint32_t maxSubdivision
) {
	assert(device != nullptr);

	// 0分割では角度計算時に0除算になるため、最低1に制限する
	maxSubdivision_ = (std::max)(maxSubdivision, 1u);

	// 最大分割数から、事前に必要な最大頂点数を計算する
	// 緯度と経度の両端を含むため、それぞれ分割数より1頂点多くなる
	const uint32_t maxVertexCount = (maxSubdivision_ + 1) * (maxSubdivision_ + 1);
	// 各区画は2三角形、合計6インデックスで構成する
	const uint32_t maxIndexCount = maxSubdivision_ * maxSubdivision_ * 6;

	// ImGuiで分割数を変えても再確保しなくてよいように最大数で確保する
	vertexBuffer_.Initialize(device, maxVertexCount);

	// 初期分割数が最大値を超えないようにする
	subdivision_ = (std::min)(16u, maxSubdivision_);

	// 最大分割数で必要になるIndex Resourceを確保する
	indexResource_ = BufferResource::Create(
		device,
		sizeof(uint32_t) * maxIndexCount
	);

	// GenerateVerticesから書き込めるように、アドレスをメンバーへ保存する
	HRESULT hr = indexResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&indexData_)
	);
	assert(SUCCEEDED(hr));

	// Index Resourceの先頭アドレスを設定する
	indexBufferView_.BufferLocation =
		indexResource_->GetGPUVirtualAddress();

	// 確保したIndex Resource全体のサイズを設定する
	indexBufferView_.SizeInBytes =
		sizeof(uint32_t) * maxIndexCount;

	// インデックスはuint32_tとして扱う
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	transform_ = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	GenerateVertices();

	// Sphere専用のMaterial Resourceを作成する
	materialResource_ =
		BufferResource::Create(
			device,
			sizeof(Material)
		);

	hr = materialResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&materialData_)
	);

	assert(SUCCEEDED(hr));

	// 最初はTextureの色をそのまま表示する
	// Sphereの色を白に設定する
	materialData_->color =
	{1.0f, 1.0f, 1.0f, 1.0f};

	// まずはSphereだけライティングを有効にする
	materialData_->enableLighting = true;

	// 初期状態ではLambertを使用する
	materialData_->lightingMode = 0;

	// Sphere専用のWVP Resourceを作成する
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

	// 座標変換用のWVPを単位行列で初期化する
	wvpData_->WVP = Matrix4x4::MakeIdentity4x4();

	// 法線変換用のWorldも単位行列で初期化する
	wvpData_->World = Matrix4x4::MakeIdentity4x4();
}

Transform& Sphere::GetTransform() {
	return transform_;
}

uint32_t Sphere::GetSubdivision() const {
	return subdivision_;
}

uint32_t Sphere::GetVertexCount() const {
	return vertexCount_;
}

void Sphere::SetSubdivision(uint32_t subdivision) {
	// 0分割を禁止し、確保済みの最大分割数以内に収める
	const uint32_t newSubdivision =
		(std::clamp)(subdivision, 1u, maxSubdivision_);

	// 同じ値なら頂点を作り直さない
	if (subdivision_ == newSubdivision) {
		return;
	}

	subdivision_ = newSubdivision;
	GenerateVertices();
}

/// <summary>
/// 更新処理
/// </summary>
/// <param name="viewMatrix"></param>
/// <param name="projectionMatrix"></param>
void Sphere::Update(const Matrix4x4& viewMatrix,
	const Matrix4x4& projectionMatrix) {
	// 常に回転する
	transform_.rotate.y += 0.02f;

	// Sphere専用のWorld行列を更新する
	const Matrix4x4 worldMatrix =
		Matrix4x4::MakeAffineMatrix(
			transform_.scale,
			transform_.rotate,
			transform_.translate
		);

	// 更新処理でSphere専用WVPを計算する
	// Sphereの頂点をクリップ空間へ変換するWVPを設定する
	wvpData_->WVP = Matrix4x4::Multiply(
		worldMatrix,
		Matrix4x4::Multiply(
		viewMatrix,
		projectionMatrix
	)
	);

	// ライティングで法線を変換するためのWorld行列を設定する
	wvpData_->World = worldMatrix;
}

/// <summary>
/// 描画処理
/// </summary>
/// <param name="commandList"></param>
/// <param name="textureHandle"></param>
void Sphere::Draw(
	ID3D12GraphicsCommandList* commandList,
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle
) const {
	assert(commandList != nullptr);
	assert(materialResource_ != nullptr);
	assert(wvpResource_ != nullptr);

	// Sphere専用VertexBufferを設定する
	const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView =
		vertexBuffer_.GetView();

	commandList->IASetVertexBuffers(
		0,
		1,
		&vertexBufferView
	);

	// SphereのIndexBufferViewを設定する
	commandList->IASetIndexBuffer(&indexBufferView_);

	commandList->IASetPrimitiveTopology(
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	);

	// Sphere専用Materialを設定する
	commandList->SetGraphicsRootConstantBufferView(
		0,
		materialResource_->GetGPUVirtualAddress()
	);

	// Sphere専用WVPを設定する
	commandList->SetGraphicsRootConstantBufferView(
		1,
		wvpResource_->GetGPUVirtualAddress()
	);

	// Sphereへ貼るTextureを設定する
	commandList->SetGraphicsRootDescriptorTable(
		2,
		textureHandle
	);

	// 現在の分割数に必要なインデックスを使ってSphereを描画する
	commandList->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
}

void Sphere::GenerateVertices() {
	// 緯度と経度の両端を含む頂点数を計算する
	const uint32_t vertexPerRow = subdivision_ + 1;
	vertexCount_ = vertexPerRow * vertexPerRow;

	// 1区画につき6インデックスを使用する
	indexCount_ = subdivision_ * subdivision_ * 6;

	// Initialize時にMapされた頂点データの書き込み先を取得する
	VertexData* vertexData = vertexBuffer_.GetData();
	assert(vertexData != nullptr);
	assert(indexData_ != nullptr);

	const float pi = std::numbers::pi_v<float>;

	// 経度と緯度の1分割分の角度
	const float lonEvery =
		pi * 2.0f / static_cast<float>(subdivision_);

	const float latEvery =
		pi / static_cast<float>(subdivision_);

	// 緯度と経度の交点ごとに、重複しない頂点データを作成する 緯度方向を-π/2からπ/2まで分割する
	// 緯度と経度の交点ごとに頂点を生成する
	for (uint32_t latIndex = 0;
		latIndex <= subdivision_;
		++latIndex) {

		const float lat =
			-pi / 2.0f +
			latEvery * static_cast<float>(latIndex);

		const float v =
			1.0f -
			static_cast<float>(latIndex) /
			static_cast<float>(subdivision_);

		for (uint32_t lonIndex = 0;
			lonIndex <= subdivision_;
			++lonIndex) {

			const float lon =
				lonEvery * static_cast<float>(lonIndex);

			const float u =
				static_cast<float>(lonIndex) /
				static_cast<float>(subdivision_);

			const Vector4 position = {
				std::cos(lat) * std::cos(lon),
				std::sin(lat),
				std::cos(lat) * std::sin(lon),
				1.0f
			};

			const uint32_t vertexIndex =
				latIndex * vertexPerRow +
				lonIndex;

			// 単位球では座標のXYZをそのまま法線として使用できる
			vertexData[vertexIndex] = {
				position,
				{u, v},
				{position.x, position.y, position.z}
			};
		}
	}

	// 生成済みの頂点番号を使って、各区画のインデックスを生成する
	for (uint32_t latIndex = 0;
		latIndex < subdivision_;
		++latIndex) {

		for (uint32_t lonIndex = 0;
			lonIndex < subdivision_;
			++lonIndex) {

			const uint32_t a =
				latIndex * vertexPerRow +
				lonIndex;

			const uint32_t b = a + 1;

			const uint32_t c =
				(latIndex + 1) * vertexPerRow +
				lonIndex;

			const uint32_t d = c + 1;

			const uint32_t indexStart =
				(latIndex * subdivision_ + lonIndex) * 6;

			// 1つ目の三角形：a-c-b
			indexData_[indexStart + 0] = a;
			indexData_[indexStart + 1] = c;
			indexData_[indexStart + 2] = b;

			// 2つ目の三角形：c-d-b
			indexData_[indexStart + 3] = c;
			indexData_[indexStart + 4] = d;
			indexData_[indexStart + 5] = b;
		}
	}
}

/// <summary>
/// 解放処理
/// </summary>
void Sphere::Finalize() {
	// Sphere専用VertexBufferを解放する
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

	subdivision_ = 0;
	maxSubdivision_ = 0;
	vertexCount_ = 0;

	// Index Resourceの書き込み先とViewを無効化する
	indexData_ = nullptr;
	indexBufferView_ = {};
	// SphereのIndex Resourceを解放する
	if (indexResource_ != nullptr) {
		indexResource_->Release();
		indexResource_ = nullptr;
	}
	indexCount_ = 0;
}
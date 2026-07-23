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
	const uint32_t maxVertexCount =
		maxSubdivision_ * maxSubdivision_ * 6;

	// ImGuiで分割数を変えても再確保しなくてよいように最大数で確保する
	vertexBuffer_.Initialize(device, maxVertexCount);

	// 初期分割数が最大値を超えないようにする
	subdivision_ = (std::min)(16u, maxSubdivision_);

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

	HRESULT hr = materialResource_->Map(
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

	commandList->DrawInstanced(
		vertexCount_,
		1,
		0,
		0
	);
}

void Sphere::GenerateVertices() {
	// 現在の分割数から、実際に描画する頂点数を計算する
	vertexCount_ = subdivision_ * subdivision_ * 6;

	// Initialize時にMapされた頂点データの書き込み先を取得する
	VertexData* vertexData = vertexBuffer_.GetData();
	assert(vertexData != nullptr);

	const float pi = std::numbers::pi_v<float>;

	// 経度と緯度の1分割分の角度
	const float lonEvery =
		pi * 2.0f / static_cast<float>(subdivision_);

	const float latEvery =
		pi / static_cast<float>(subdivision_);

	// 緯度方向を-π/2からπ/2まで分割する
	for (uint32_t latIndex = 0;
		latIndex < subdivision_;
		++latIndex) {

		const float lat0 =
			-pi / 2.0f + latEvery * latIndex;

		const float lat1 = lat0 + latEvery;

		// 球の下がv=1、上がv=0になるように計算する
		const float v0 =
			1.0f -
			static_cast<float>(latIndex) /
			static_cast<float>(subdivision_);

		const float v1 =
			1.0f -
			static_cast<float>(latIndex + 1) /
			static_cast<float>(subdivision_);

		// 経度方向を0から2πまで分割する
		for (uint32_t lonIndex = 0;
			lonIndex < subdivision_;
			++lonIndex) {

			const float lon0 = lonEvery * lonIndex;
			const float lon1 = lon0 + lonEvery;

			const float u0 =
				static_cast<float>(lonIndex) /
				static_cast<float>(subdivision_);

			const float u1 =
				static_cast<float>(lonIndex + 1) /
				static_cast<float>(subdivision_);

			// 1区画6頂点の書き込み開始位置
			const uint32_t startIndex =
				(latIndex * subdivision_ + lonIndex) * 6;

			// 1区画を構成する4つの基準点
			const Vector4 a = {
				std::cos(lat0) * std::cos(lon0),
				std::sin(lat0),
				std::cos(lat0) * std::sin(lon0),
				1.0f
			};

			const Vector4 b = {
				std::cos(lat0) * std::cos(lon1),
				std::sin(lat0),
				std::cos(lat0) * std::sin(lon1),
				1.0f
			};

			const Vector4 c = {
				std::cos(lat1) * std::cos(lon0),
				std::sin(lat1),
				std::cos(lat1) * std::sin(lon0),
				1.0f
			};

			const Vector4 d = {
				std::cos(lat1) * std::cos(lon1),
				std::sin(lat1),
				std::cos(lat1) * std::sin(lon1),
				1.0f
			};

			/*
			// 1枚目の三角形 a-b-c
			vertexData[startIndex + 0] = {
				a, {u0, v0}
			};
			vertexData[startIndex + 1] = {
				b, {u1, v0}
			};
			vertexData[startIndex + 2] = {
				c, {u0, v1}
			};

			// 2枚目の三角形 c-b-d
			vertexData[startIndex + 3] = {
				c, {u0, v1}
			};
			vertexData[startIndex + 4] = {
				b, {u1, v0}
			};
			vertexData[startIndex + 5] = {
				d, {u1, v1}
			};
			*/

			// 1枚目_a-c-b
			// 単位球では頂点座標のXYZが外向き法線になる
			vertexData[startIndex + 0] = {
				a,
				{u0, v0},
				{a.x, a.y, a.z}
			};

			vertexData[startIndex + 1] = {
				c,
				{u0, v1},
				{c.x, c.y, c.z}
			};

			vertexData[startIndex + 2] = {
				b,
				{u1, v0},
				{b.x, b.y, b.z}
			};

			// 2枚目_c-d-b
			vertexData[startIndex + 3] = {
				c,
				{u0, v1},
				{c.x, c.y, c.z}
			};

			vertexData[startIndex + 4] = {
				d,
				{u1, v1},
				{d.x, d.y, d.z}
			};

			vertexData[startIndex + 5] = {
				b,
				{u1, v0},
				{b.x, b.y, b.z}
			};
			
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
}
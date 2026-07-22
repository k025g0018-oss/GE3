#include "Primitive3D.h"

#include "BufferResource.h"

#include <cassert>
#include <cmath>

namespace {
// 三角形1枚の基準頂点。2枚表示でもこの形を複製して使用する
constexpr VertexData kTriangleVertices[3] = {
	{{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}},
	{{0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}},
	{{-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f}},
};

// 三角錐1個の基準頂点。側面3枚と底面1枚を合計12頂点で作る
constexpr VertexData kPyramidVertices[12] = {
	{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, {{0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}},
	{{0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, {{0.0f, -0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}},
	{{0.0f, -0.5f, 0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, {{-0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}},
	{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 0.0f}}, {{0.0f, -0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}},
};
}

Primitive3D::~Primitive3D() {
	// main側でFinalize済みでも安全なよう、nullptr確認を行うFinalizeへ統一する
	Finalize();
}

void Primitive3D::Initialize(ID3D12Device* device) {
	assert(device != nullptr);

	// 最も頂点数が多い三角錐2個分のVertexBufferを最初に確保する
	vertexBuffer_.Initialize(device, kMaxVertexCount);

	// Material用の定数バッファを作り、初期色を白にする
	materialResource_ = BufferResource::Create(device, sizeof(Vector4));
	HRESULT hr = materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	assert(SUCCEEDED(hr));
	*materialData_ = {1.0f, 1.0f, 1.0f, 1.0f};

	// Primitive3D専用のWVP定数バッファを作る
	wvpResource_ = BufferResource::Create(device, sizeof(Matrix4x4));
	hr = wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
	assert(SUCCEEDED(hr));
	*wvpData_ = Matrix4x4::MakeIdentity4x4();
	Reset();
}

void Primitive3D::Update(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix) {
	// Start中だけ図形全体をY軸方向へ回転させる
	if (isPlaying_) {
		transform_.rotate.y += 0.01f;
	}

	// ImGuiで変更されたモードや個別Transformを頂点へ反映する
	GenerateVertices();

	// Primitive3D専用のWorld行列からWVPを計算する
	const Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	*wvpData_ = Matrix4x4::Multiply(worldMatrix, Matrix4x4::Multiply(viewMatrix, projectionMatrix));
}

void Primitive3D::Draw(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle) const {
	assert(commandList != nullptr);

	// NoneまたはParticleSystemモードではPrimitive3Dを描画しない
	if (drawVertexCount_ == 0 || displayMode_ == 5) {
		return;
	}

	// Primitive3D専用の頂点、Material、WVP、Textureを描画パイプラインへ設定する
	const D3D12_VERTEX_BUFFER_VIEW& view = vertexBuffer_.GetView();
	commandList->IASetVertexBuffers(0, 1, &view);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(2, textureHandle);
	commandList->DrawInstanced(drawVertexCount_, 1, 0, 0);
}

VertexData Primitive3D::ApplyLocalTransform(const VertexData& vertex, const float* scale, const float* rotate, const float* translate) const {
	// 基準頂点を直接変更せず、コピーした頂点へ個別Transformを適用する
	VertexData result = vertex;

	// Scaleを先に適用する
	float x = result.position.x * scale[0];
	float y = result.position.y * scale[1];
	float z = result.position.z * scale[2];
	// X、Y、Zの順番で回転を適用する
	const float cosX = std::cos(rotate[0]); const float sinX = std::sin(rotate[0]);
	float nextY = y * cosX - z * sinX; float nextZ = y * sinX + z * cosX; y = nextY; z = nextZ;
	const float cosY = std::cos(rotate[1]); const float sinY = std::sin(rotate[1]);
	float nextX = x * cosY + z * sinY; nextZ = -x * sinY + z * cosY; x = nextX; z = nextZ;
	const float cosZ = std::cos(rotate[2]); const float sinZ = std::sin(rotate[2]);
	nextX = x * cosZ - y * sinZ; nextY = x * sinZ + y * cosZ;
	// 最後にTranslateを加えて個別Transformを完成させる
	result.position.x = nextX + translate[0];
	result.position.y = nextY + translate[1];
	result.position.z = z + translate[2];
	return result;
}

void Primitive3D::GenerateVertices() {
	VertexData* data = vertexBuffer_.GetData();
	// Noneの場合に以前の頂点数が残らないよう、毎回0から設定する
	drawVertexCount_ = 0;

	// モード1：三角形を1枚、そのままVertexBufferへ書き込む
	if (displayMode_ == 1) {
		drawVertexCount_ = 3;
		for (uint32_t i = 0; i < 3; ++i) { data[i] = kTriangleVertices[i]; }
	// モード2：三角形を2枚作り、それぞれへ個別Transformを適用する
	} else if (displayMode_ == 2) {
		drawVertexCount_ = 6;
		for (uint32_t i = 0; i < 3; ++i) {
			data[i] = ApplyLocalTransform(kTriangleVertices[i], triangle1Scale_, triangle1Rotate_, triangle1Translate_);
			data[i + 3] = ApplyLocalTransform(kTriangleVertices[i], triangle2Scale_, triangle2Rotate_, triangle2Translate_);
		}
	// モード3：三角錐を1個、そのままVertexBufferへ書き込む
	} else if (displayMode_ == 3) {
		drawVertexCount_ = 12;
		for (uint32_t i = 0; i < 12; ++i) { data[i] = kPyramidVertices[i]; }
	// モード4：三角錐を2個作り、それぞれへ個別Transformを適用する
	} else if (displayMode_ == 4) {
		drawVertexCount_ = 24;
		for (uint32_t i = 0; i < 12; ++i) {
			data[i] = ApplyLocalTransform(kPyramidVertices[i], pyramid1Scale_, pyramid1Rotate_, pyramid1Translate_);
			data[i + 12] = ApplyLocalTransform(kPyramidVertices[i], pyramid2Scale_, pyramid2Rotate_, pyramid2Translate_);
		}
	}
}

void Primitive3D::Reset() {
	// displayMode_は変更せず、現在表示しているモードの初期状態へ戻す
	transform_ = {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
	isPlaying_ = false;

	// すべての個別Transformを初期値へ戻す
	for (int i = 0; i < 3; ++i) {
		triangle1Scale_[i] = triangle2Scale_[i] = pyramid1Scale_[i] = pyramid2Scale_[i] = 1.0f;
		triangle1Rotate_[i] = triangle2Rotate_[i] = pyramid1Rotate_[i] = pyramid2Rotate_[i] = 0.0f;
		triangle1Translate_[i] = triangle2Translate_[i] = pyramid1Translate_[i] = pyramid2Translate_[i] = 0.0f;
	}
	// 以前のモード2と同じ配置へ戻し、2枚の三角形が完全に重ならないようにする
	triangle1Translate_[0] = -0.2f;
	triangle1Translate_[1] = -0.2f;
	triangle1Translate_[2] = 0.0f;
	triangle2Translate_[0] = 0.2f;
	triangle2Translate_[1] = 0.2f;
	triangle2Translate_[2] = 0.2f;
	// 三角錐2個が重ならないよう、初期X座標だけ左右へ分ける
	pyramid1Translate_[0] = -0.3f;
	pyramid2Translate_[0] = 0.3f;
	GenerateVertices();
}

void Primitive3D::Finalize() {
	// VertexBufferが所有する頂点リソースを解放する
	vertexBuffer_.Finalize();

	// Map先を無効にしてからMaterialリソースを解放する
	materialData_ = nullptr;
	if (materialResource_ != nullptr) { materialResource_->Release(); materialResource_ = nullptr; }

	// Map先を無効にしてからWVPリソースを解放する
	wvpData_ = nullptr;
	if (wvpResource_ != nullptr) { wvpResource_->Release(); wvpResource_ = nullptr; }
	drawVertexCount_ = 0;
}

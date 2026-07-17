#include "ParticleSystem.h"

#include "BufferResource.h"

#include <algorithm>
#include <cassert>
#include <cmath>

///// ----- ParticleSystem ----- /////

ParticleSystem::~ParticleSystem() {
	Finalize();
}

/// --- 初期化 ---
// 移動範囲と最大数を設定し、最初のParticleを生成
void ParticleSystem::Initialize(ID3D12Device* device, float fieldMin, float fieldMax, uint32_t maxParticleCount) {
	assert(device != nullptr);
	assert(fieldMin < fieldMax);
	assert(maxParticleCount > 0);

	fieldMin_ = fieldMin;
	fieldMax_ = fieldMax;
	maxParticleCount_ = maxParticleCount;

	// 乱数生成器は初期化時に1回だけ作る
	std::random_device randomDevice;
	randomEngine_.seed(randomDevice());

	// Particleごとに256バイト境界のWVP領域を用意する
	particleWvpResource_ = BufferResource::Create(
		device,
		static_cast<size_t>(kConstantBufferAlignment) * maxParticleCount_
	);

	HRESULT hr = particleWvpResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&particleWvpData_)
	);
	assert(SUCCEEDED(hr));

	particles_.reserve(maxParticleCount_);
	Reset();
}

/// --- 更新 ---
// 移動、回転、壁反射、Particleの追加を行う
void ParticleSystem::Update() {
	// このフレームで通過した処理をParticle Flowパネル用に記録する
	debugFlowState_ = {};
	debugFlowState_.updateExecuted = true;
	debugFlowState_.particleCountBefore = particles_.size();

	if (particles_.empty()) {
		Reset();
	}

	uint32_t spawnCount = 0;
	const size_t updateParticleCount = particles_.size();
	debugFlowState_.moveExecuted = updateParticleCount > 0;
	debugFlowState_.collisionChecked = updateParticleCount > 0;

	// 1_現在存在するParticleを移動する
	for (size_t i = 0; i < updateParticleCount; ++i) {
		Particle& particle = particles_[i];
		particle.position = particle.position + particle.velocity;
		particle.rotate.z += particle.angularVelocity;

		// 2_壁に当たったParticleを反射する
		const bool collided = ReflectAtFieldWall(particle);
		if (collided) {
			debugFlowState_.collisionCount++;
		}

		// 3_壁に当たった数だけ新しいParticleを追加する
		if (collided && particles_.size() + spawnCount < maxParticleCount_) {
			spawnCount++;
		}
	}

	for (uint32_t i = 0; i < spawnCount; ++i) {
		particles_.push_back(CreateParticle());
	}
	debugFlowState_.spawnCount = spawnCount;

	// 4_最大数に到達したら全て削除して1個から再開する
	if (particles_.size() >= maxParticleCount_) {
		Reset();
	}

	debugFlowState_.particleCountAfter = particles_.size();
}

/// --- 頂点データ ---
// Particleで使用する三角形の頂点を書き込む
void ParticleSystem::WriteTriangleVertices(VertexData* vertexData) const {
	assert(vertexData != nullptr);

	const VertexData triangleVertices[3] = {
		{{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}},
		{{0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}},
		{{-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f}},
	};

	for (uint32_t i = 0; i < 3; ++i) {
		vertexData[i] = triangleVertices[i];
	}
}

/// --- 描画 ---
// ParticleごとのWVPを設定して三角形を描画
void ParticleSystem::Draw(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix) {
	assert(commandList != nullptr);

	for (size_t i = 0; i < particles_.size(); ++i) {
		const Particle& particle = particles_[i];

		const Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(
			particle.scale,
			particle.rotate,
			particle.position
		);

		const Matrix4x4 particleWvp = Matrix4x4::Multiply(
			worldMatrix,
			Matrix4x4::Multiply(viewMatrix, projectionMatrix)
		);

		// Particleごとに別の256バイト領域へWVPを書き込む
		uint8_t* currentWvpData = particleWvpData_ + (kConstantBufferAlignment * i);
		*reinterpret_cast<Matrix4x4*>(currentWvpData) = particleWvp;

		// Particleごとに別のConstantBufferViewを指定する
		const D3D12_GPU_VIRTUAL_ADDRESS currentWvpAddress =
			particleWvpResource_->GetGPUVirtualAddress() + (kConstantBufferAlignment * i);
		commandList->SetGraphicsRootConstantBufferView(1, currentWvpAddress);

		// 3頂点で1つの三角形を描画する
		commandList->DrawInstanced(3, 1, 0, 0);
	}
}

/// --- リセット ---
// 全Particleを削除して1個目を生成
void ParticleSystem::Reset() {
	// 手動Resetと最大数到達ResetのどちらもFlowパネルへ通知する
	debugFlowState_.resetOccurred = true;
	particles_.clear();
	if (maxParticleCount_ > 0) {
		particles_.push_back(CreateParticle());
	}
}

/// --- 取得 ---
// 先頭のParticleを取得
const ParticleSystem::Particle* ParticleSystem::GetFirstParticle() const {
	if (particles_.empty()) {
		return nullptr;
	}
	return &particles_.front();
}

/// --- Particleの生成 ---
// ランダムな位置、方向、速さ、回転でParticleを作成
ParticleSystem::Particle ParticleSystem::CreateParticle() {
	constexpr float kPi = 3.14159265358979323846f;

	std::uniform_real_distribution<float> scaleDist(0.15f, 0.35f);
	std::uniform_real_distribution<float> directionDist(0.0f, kPi * 2.0f);
	std::uniform_real_distribution<float> speedDist(0.005f, 0.025f);
	std::uniform_real_distribution<float> rotateDist(-kPi, kPi);
	std::uniform_real_distribution<float> angularVelocityDist(-0.02f, 0.02f);

	Particle particle{};
	particle.scale = {
		scaleDist(randomEngine_),
		scaleDist(randomEngine_),
		1.0f
	};

	// 三角形全体が範囲内に入る位置から生成する
	const float collisionRadius = (std::max)(particle.scale.x, particle.scale.y) * 0.5f;
	std::uniform_real_distribution<float> positionDist(
		fieldMin_ + collisionRadius,
		fieldMax_ - collisionRadius
	);
	particle.position = {
		positionDist(randomEngine_),
		positionDist(randomEngine_),
		0.0f
	};

	// 方向と速さを別々にランダム化して停止しないようにする
	const float direction = directionDist(randomEngine_);
	const float speed = speedDist(randomEngine_);
	particle.velocity = {
		std::cos(direction) * speed,
		std::sin(direction) * speed,
		0.0f
	};

	particle.rotate = {0.0f, 0.0f, rotateDist(randomEngine_)};
	particle.angularVelocity = angularVelocityDist(randomEngine_);

	return particle;
}

/// --- 壁との反射 ---
// Particleを範囲内へ戻し、移動方向を反転
bool ParticleSystem::ReflectAtFieldWall(Particle& particle) {
	bool collided = false;
	const float collisionRadius = (std::max)(particle.scale.x, particle.scale.y) * 0.5f;
	const float minPosition = fieldMin_ + collisionRadius;
	const float maxPosition = fieldMax_ - collisionRadius;

	// X方向の壁に当たったら座標を範囲内へ戻して反射する
	if (particle.position.x < minPosition) {
		particle.position.x = minPosition;
		particle.velocity.x = std::abs(particle.velocity.x);
		collided = true;
	} else if (particle.position.x > maxPosition) {
		particle.position.x = maxPosition;
		particle.velocity.x = -std::abs(particle.velocity.x);
		collided = true;
	}

	// Y方向の壁に当たったら座標を範囲内へ戻して反射する
	if (particle.position.y < minPosition) {
		particle.position.y = minPosition;
		particle.velocity.y = std::abs(particle.velocity.y);
		collided = true;
	} else if (particle.position.y > maxPosition) {
		particle.position.y = maxPosition;
		particle.velocity.y = -std::abs(particle.velocity.y);
		collided = true;
	}

	return collided;
}

/// --- 終了処理 ---
// Particle用のWVPResourceを解放
void ParticleSystem::Finalize() {
	particles_.clear();
	particleWvpData_ = nullptr;
	if (particleWvpResource_ != nullptr) {
		particleWvpResource_->Release();
		particleWvpResource_ = nullptr;
	}
	maxParticleCount_ = 0;
}

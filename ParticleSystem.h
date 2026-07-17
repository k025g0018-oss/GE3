#pragma once

#include "Matrix4x4.h"
#include "VertexData.h"

#include <d3d12.h>
#include <cstdint>
#include <random>
#include <vector>

///// ----- ParticleSystem ----- /////

class ParticleSystem {
public:
	/// --- Particle ---
	// 1個の三角形が持つ情報
	struct Particle {
		Vector3 position;
		Vector3 velocity;
		Vector3 scale;
		Vector3 rotate;
		float angularVelocity;
	};

	// Particle Flowパネルへ渡す1フレーム分の実行情報
	struct DebugFlowState {
		bool updateExecuted = false;
		bool moveExecuted = false;
		bool collisionChecked = false;
		bool resetOccurred = false;
		uint32_t collisionCount = 0;
		uint32_t spawnCount = 0;
		size_t particleCountBefore = 0;
		size_t particleCountAfter = 0;
	};

	ParticleSystem() = default;
	~ParticleSystem();

	ParticleSystem(const ParticleSystem&) = delete;
	ParticleSystem& operator=(const ParticleSystem&) = delete;

	/// --- 初期化 ---
	// 移動範囲と最大数を設定し、最初のParticleを生成
	void Initialize(ID3D12Device* device, float fieldMin, float fieldMax, uint32_t maxParticleCount);

	/// --- 更新 ---
	// 移動、回転、壁反射、Particleの追加を行う
	void Update();

	/// --- 頂点データ ---
	// Particleで使用する三角形の頂点を書き込む
	void WriteTriangleVertices(VertexData* vertexData) const;

	/// --- 描画 ---
	// ParticleごとのWVPを設定して三角形を描画
	void Draw(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix);

	/// --- リセット ---
	// 全Particleを削除して1個目を生成
	void Reset();

	/// --- 取得 ---
	// 現在のParticle数を取得
	size_t GetParticleCount() const { return particles_.size(); }

	// 描画する頂点数の合計を取得
	size_t GetTotalVertexCount() const { return particles_.size() * 3; }

	// 先頭のParticleを取得
	const Particle* GetFirstParticle() const;

	// Particle Flowパネル用の実行情報を取得する
	const DebugFlowState& GetDebugFlowState() const { return debugFlowState_; }

	// 設定されているParticle最大数を取得する
	uint32_t GetMaxParticleCount() const { return maxParticleCount_; }

	/// --- 終了処理 ---
	// Particle用のWVPResourceを解放
	void Finalize();

private:
	/// --- Particleの生成 ---
	// ランダムな位置、方向、速さ、回転でParticleを作成
	Particle CreateParticle();

	/// --- 壁との反射 ---
	// Particleを範囲内へ戻し、移動方向を反転
	bool ReflectAtFieldWall(Particle& particle);

	static constexpr uint32_t kConstantBufferAlignment = 256;

	std::vector<Particle> particles_;
	std::mt19937 randomEngine_;
	float fieldMin_ = -0.9f;
	float fieldMax_ = 0.9f;
	uint32_t maxParticleCount_ = 0;
	DebugFlowState debugFlowState_{};

	ID3D12Resource* particleWvpResource_ = nullptr;
	uint8_t* particleWvpData_ = nullptr;
};

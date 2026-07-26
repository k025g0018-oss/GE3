#pragma once

#include "BlendState.h"
#include "DepthStencilState.h"
#include "InputLayout.h"
#include "RasterizerState.h"
#include "RootSignature.h"
#include "ShaderProgram.h"

#include <d3d12.h>
#include <wrl.h>
#include <ostream>

///// ----- PSO(Pipeline State Object) ----- /////

class PipelineState {
public:
	PipelineState() = default;
	~PipelineState();

	PipelineState(const PipelineState&) = delete;
	PipelineState& operator=(const PipelineState&) = delete;

	/// --- 初期化 ---
	// 描画に必要なPSOを作成
	void Initialize(ID3D12Device* device, std::ostream& logStream);

	/// --- 取得 ---
	// 作成したRootSignatureを取得
	ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }

	// 作成したPSOを取得
	ID3D12PipelineState* GetPipelineState() const { return graphicsPipelineState_.Get(); }

	/// --- 終了処理 ---
	// PSOで使用したリソースを解放
	void Finalize();

private:
	RootSignature rootSignature_;
	InputLayout inputLayout_;
	BlendState blendState_;
	RasterizerState rasterizerState_;
	DepthStencilState depthStencilState_;
	ShaderProgram shaderProgram_;
	// PipelineStateの寿命をComPtrで管理する
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
};

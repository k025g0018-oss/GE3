#include "PipelineState.h"

#include <cassert>

///// ----- PSO(Pipeline State Object) ----- /////

PipelineState::~PipelineState() {
	Finalize();
}

/// --- 初期化 ---
// 描画に必要なPSOを作成
void PipelineState::Initialize(ID3D12Device* device, std::ostream& logStream) {
	/// --- RootSignature ---
	// RootSignatureを作成
	rootSignature_.Initialize(device, logStream);

	/// --- InputLayout ---
	// 頂点データの並びを設定
	inputLayout_.Initialize();

	/// --- BlendState ---
	// 色の書き込み方法を設定
	blendState_.Initialize();

	/// --- RasterizerState ---
	// 面の判定と塗りつぶし方法を設定
	rasterizerState_.Initialize();

	/// --- ShaderProgram ---
	// VertexShaderとPixelShaderをコンパイル
	shaderProgram_.Initialize(logStream);

	/// --- DepthStencilState ---
	// 奥行きの比較と書き込み方法を設定
	depthStencilState_.Initialize();

	/// --- GraphicsPipelineState ---
	// PSOを生成する
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get(); // RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayout_.GetDesc(); // InputLayout
	graphicsPipelineStateDesc.VS = shaderProgram_.GetVertexShaderByteCode(); // VertexShader
	graphicsPipelineStateDesc.PS = shaderProgram_.GetPixelShaderByteCode(); // PixelShader
	graphicsPipelineStateDesc.BlendState = blendState_.GetDesc(); // BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerState_.GetDesc(); // RasterizerState
	// DepthStencilの設定、Depthを使いますよという意思表示
	graphicsPipelineStateDesc.DepthStencilState = depthStencilState_.GetDesc();
	graphicsPipelineStateDesc.DSVFormat = depthStencilState_.GetFormat();

	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトロポジ(形状)のタイプ、三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// 実際に生成
	HRESULT hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr));
}

/// --- 終了処理 ---
// PSO関連のリソースを解放
void PipelineState::Finalize() {
	if (graphicsPipelineState_ != nullptr) {
		graphicsPipelineState_->Release();
		graphicsPipelineState_ = nullptr;
	}
	shaderProgram_.Finalize();
	rootSignature_.Finalize();
}

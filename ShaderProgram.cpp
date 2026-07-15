#include "ShaderProgram.h"

#include <cassert>

///// ----- ShaderProgram ----- /////

ShaderProgram::~ShaderProgram() {
	Finalize();
}

/// --- 初期化 ---
// VertexShaderとPixelShaderをコンパイル
void ShaderProgram::Initialize(std::ostream& logStream) {
	// DXCを初期化
	dxcCompiler_.Initialize();

	// ShaderをCompileする
	vertexShaderBlob_ = dxcCompiler_.CompileShader(L"Object3D.VS.hlsl", L"vs_6_0", logStream);
	assert(vertexShaderBlob_ != nullptr);

	pixelShaderBlob_ = dxcCompiler_.CompileShader(L"Object3D.PS.hlsl", L"ps_6_0", logStream);
	assert(pixelShaderBlob_ != nullptr);
}

/// --- 取得 ---
// VertexShaderのByteCodeを取得
D3D12_SHADER_BYTECODE ShaderProgram::GetVertexShaderByteCode() const {
	return {vertexShaderBlob_->GetBufferPointer(), vertexShaderBlob_->GetBufferSize()};
}

// PixelShaderのByteCodeを取得
D3D12_SHADER_BYTECODE ShaderProgram::GetPixelShaderByteCode() const {
	return {pixelShaderBlob_->GetBufferPointer(), pixelShaderBlob_->GetBufferSize()};
}

/// --- 終了処理 ---
// Shaderで使用したリソースを解放
void ShaderProgram::Finalize() {
	if (vertexShaderBlob_ != nullptr) {
		vertexShaderBlob_->Release();
		vertexShaderBlob_ = nullptr;
	}
	if (pixelShaderBlob_ != nullptr) {
		pixelShaderBlob_->Release();
		pixelShaderBlob_ = nullptr;
	}
	dxcCompiler_.Finalize();
}

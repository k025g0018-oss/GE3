#pragma once

#include "DxcCompiler.h"

#include <d3d12.h>
#include <ostream>

///// ----- ShaderProgram ----- /////

class ShaderProgram {
public:
	ShaderProgram() = default;
	~ShaderProgram();

	ShaderProgram(const ShaderProgram&) = delete;
	ShaderProgram& operator=(const ShaderProgram&) = delete;

	/// --- 初期化 ---
	// VertexShaderとPixelShaderをコンパイル
	void Initialize(std::ostream& logStream);

	/// --- 取得 ---
	// VertexShaderのByteCodeを取得
	D3D12_SHADER_BYTECODE GetVertexShaderByteCode() const;

	// PixelShaderのByteCodeを取得
	D3D12_SHADER_BYTECODE GetPixelShaderByteCode() const;

	/// --- 終了処理 ---
	// Shaderで使用したリソースを解放
	void Finalize();

private:
	DxcCompiler dxcCompiler_;
	IDxcBlob* vertexShaderBlob_ = nullptr;
	IDxcBlob* pixelShaderBlob_ = nullptr;
};

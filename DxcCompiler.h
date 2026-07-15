#pragma once

#include <Windows.h>
#include <dxcapi.h>
#include <ostream>
#include <string>

///// ----- DXC(DirectX Shader Compiler) ----- /////

class DxcCompiler {
public:
	DxcCompiler() = default;
	~DxcCompiler();

	DxcCompiler(const DxcCompiler&) = delete;
	DxcCompiler& operator=(const DxcCompiler&) = delete;

	/// --- 初期化 ---
	// DXCを初期化
	void Initialize();

	/// --- Shaderのコンパイル ---
	// HLSLをコンパイルして実行用のバイナリを返す
	IDxcBlob* CompileShader(
		// CompilerするShaderファイルへのパス
		const std::wstring& filePath,
		// Compilerに使用するProfile
		const wchar_t* profile,
		std::ostream& logStream
	);

	/// --- 終了処理 ---
	// DXCで使用したリソースを解放
	void Finalize();

private:
	// 初期化で生成したものを3つ
	IDxcUtils* dxcUtils_ = nullptr;
	IDxcCompiler3* dxcCompiler_ = nullptr;
	IDxcIncludeHandler* includeHandler_ = nullptr;
};

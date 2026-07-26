#pragma once

#include <Windows.h>
#include <dxcapi.h>
#include <ostream>
#include <string>
#include <wrl.h>

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
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
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
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
};

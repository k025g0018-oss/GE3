#include "DxcCompiler.h"

#include "Logger.h"

#include <cassert>
#include <format>

///// ----- DXC(DirectX Shader Compiler) ----- /////

DxcCompiler::~DxcCompiler() {
	Finalize();
}

/// --- 初期化 ---
// dxcCompilerを初期化
void DxcCompiler::Initialize() {
	HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	assert(SUCCEEDED(hr));

	// 現時点でincludeはしないが、includeに対応するための設定を行っておく
	hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	assert(SUCCEEDED(hr));
}

/// --- Shaderのコンパイル ---
// CompileShader関数
IDxcBlob* DxcCompiler::CompileShader(const std::wstring& filePath, const wchar_t* profile, std::ostream& logStream) {
	/// --- HLSLファイルの読み込み ---
	// 1_HLSLファイルを読む
	// シェーダーをコンパイルする旨をログに出す
	Log(logStream, ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));
	// hlslファイルを読む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	// 読めなかったら止める
	assert(SUCCEEDED(hr));
	// 読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8; // UTF8の文字コードであることを通知

	/// --- Compile ---
	// 2_Compileする
	LPCUWSTR arguments[] = {
		filePath.c_str(), // コンパイル対象のhlslファイル名
		L"-E", L"main", // エントリーポイントの指定。基本的にmain以外にはしない
		L"-T", profile, // ShaderProfileの設定
		L"-Zi", L"-Qembed_debug", // デバッグ用の情報を埋め込む
		L"-Od", // 最適化を外しておく
		L"-Zpr", // メモリレイアウトは行優先
	};

	// 実際にShaderをコンパイルする
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler_->Compile(
		&shaderSourceBuffer, // 読み込んだファイル
		(LPCWSTR*)arguments, // コンパイルオプション
		(UINT32)_countof(arguments), // コンパイルオプションの数
		includeHandler_, // includeが含まれた諸々
		IID_PPV_ARGS(&shaderResult) // コンパイル結果
	);

	// コンパイルエラー出なくdxcが起動できないなど致命的な状況
	assert(SUCCEEDED(hr));

	/// --- 警告とエラーの確認 ---
	// 3_警告・エラーが出ていないか確認する
	// 出ていたらログに出して止める
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(logStream, shaderError->GetStringPointer());
		// 警告・エラー
		assert(false);
	}
	if (shaderError != nullptr) {
		shaderError->Release();
	}

	/// --- Compile結果の取得 ---
	// 4_Compile結果を受け取って返す
	// コンパイル結果から実行用のバイナリ部分を取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	// 成功したログを出す
	Log(logStream, ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));
	// もう使わないリソースを開放
	shaderSource->Release();
	shaderResult->Release();
	// 実行用のバイナリを返却
	return shaderBlob;
}

/// --- 終了処理 ---
// DXC関連のツールを解放
void DxcCompiler::Finalize() {
	if (includeHandler_ != nullptr) {
		includeHandler_->Release();
		includeHandler_ = nullptr;
	}
	if (dxcCompiler_ != nullptr) {
		dxcCompiler_->Release();
		dxcCompiler_ = nullptr;
	}
	if (dxcUtils_ != nullptr) {
		dxcUtils_->Release();
		dxcUtils_ = nullptr;
	}
}

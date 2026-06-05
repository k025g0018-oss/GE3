#include <windows.h>
#include <cstdint> // int32_t
#include <string> // 文字列
#include <format>
#include <filesystem> // ファイルやディレクトリに関する操作を行うライブラリ
#include <fstream> // ファイルに書いたり読んだりするライブラリ
#include <chrono> // 時間を扱うライブラリ
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <dbghelp.h> // Debug用のあれやこれやを使えるようにする
#include <strsafe.h> // StringCchPrintfWの利用に必要
#include <dxgidebug.h>
#include <dxcapi.h>
#include "Matrix4x4.h"
#include "Vector.h"
#include <vector>
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"

// ImGui
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif // USE_IMGUI

// libのリンク
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib") // Debug用のあれやこれやを使えるようにする
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

/// --- 構造体 ---
// 頂点データの拡張
struct VertexData {
	Vector4 position;
	Vector2 texcoord;
};

/// --- 関数の定義エリア ---
#pragma region
// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	// ImGui
#if USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif // USE_IMGUI

	// メッセージに応じてゲーム固有の処理を行う
	switch (msg) {
		// ウィンドウが破棄された
		case WM_DESTROY:
			// OSに対して、アプリの終了を伝える
			PostQuitMessage(0);
			return 0;
	}

	// 標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

// 文字列変換用
// ConvertString
std::wstring ConvertString(const std::string& str) {
	if (str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0) {
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

std::string ConvertString(const std::wstring& str) {
	if (str.empty()) {
		return std::string();
	}

	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	if (sizeNeeded == 0) {
		return std::string();
	}
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
	return result;
}

// Log関数
void Log(std::ostream& os, const std::string& message) {
	os << message << std::endl;
	OutputDebugStringA(message.c_str());
}

// ワイド文字版のLog関数
void Log(std::ostream& os, const std::wstring& messege) {
	std::string str = ConvertString(messege);
	os << str << std::endl;
	OutputDebugStringA(str.c_str());
}

// CrashHandlerの登録
static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
	// Dumpを出力する
	// 時刻を取得して、時刻を名前に入れたファイルを作成。Dumpsディレクトリ以下に出力
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = {0};
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",
		time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
	// processId(このexeのId)とクラッシュ(例外)の発生したthreadIdを取得
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();
	// 設定情報を入力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{0};
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;
	// Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
	// 他に関連付けられているSEH例外ハンドラがあれば実行。通常はプロセスを終了する。
	return EXCEPTION_EXECUTE_HANDLER;
}

// CompileShader関数
IDxcBlob* CompileShader(
	// CompilerするShaderファイルへのパス
	const std::wstring& filePath,
	// Compilerに使用するProfile
	const wchar_t* profile,
	// 初期化で生成したものを3つ
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler,
	std::ostream& logStream
) {
	/// 1.hlslファイルを読む
	// シェーダーをコンパイルする旨をログに出す
	Log(logStream, ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));
	// hlslファイルを読む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	// 読めなかったら止める
	assert(SUCCEEDED(hr));
	// 読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8; // UTF8の文字コードであることを通知

	/// 2.Compileする
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
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer, // 読み込んだファイル
		(LPCWSTR*)arguments, // コンパイルオプション
		(UINT32)_countof(arguments), // コンパイルオプションの数
		includeHandler, // includeが含まれた諸々
		IID_PPV_ARGS(&shaderResult) // コンパイル結果
	);

	// コンパイルエラー出なくdxcが起動できないなど致命的な状況
	assert(SUCCEEDED(hr));

	/// 3.警告・エラーが出ていないか確認する
	// 出ていたらログに出して止める
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(logStream, shaderError->GetStringPointer());
		// 警告・エラー
		assert(false);
	}

	/// 4.Compile結果を受け取って返す
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

// バッファリソースを作る関数
ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {

	size_t alignedSize = (sizeInBytes + 255) & ~255;
	// 頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	// 頂点リソースの設定
	D3D12_RESOURCE_DESC vertexResourceDesc{};

	// バッファリソース
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width = alignedSize;

	// バッファの場合はこれらを1にする
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 実際に頂点リソースを作る
	ID3D12Resource* vertexResource = nullptr;
	HRESULT hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexResource));
	assert(SUCCEEDED(hr));

	return vertexResource;
}

// ID3D12DescriptorHeapの作成関数
ID3D12DescriptorHeap* CreateDesdcriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
	// ディスクリプタヒープの生成
	ID3D12DescriptorHeap* descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType; // ヒープタイプ
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));

	return descriptorHeap;
}

// Textureデータを読むための関数
DirectX::ScratchImage LoadTexture(const std::string& filePath) {
	// テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	// ミップマップの作製
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	// ミップマップ付きのデータを返す
	return mipImages;
}

// TextureResourceを作る
ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata) {
	// 1_metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width); // Textureの幅
	resourceDesc.Height = UINT(metadata.height); // Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels); // mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize); // 奥行or配列Textureの配列数
	resourceDesc.Format = metadata.format; // TextureのFormat
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension); // Textureの次元数

	// 2_利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // 細かい設定を行う

	// 3_Resourceを生成する
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定、特になし
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST, // 初回のResourceState
		nullptr, // Clear最適値、でも使わない
		IID_PPV_ARGS(&resource)
	);
	assert(SUCCEEDED(hr));

	return resource;
}

// TextureResourceにデータを転送する
[[nodiscard]]
ID3D12Resource* UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList) {

	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);

	uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));

	// 中継用のバッファ（UPLOAD）を作成
	ID3D12Resource* intermediateResource = CreateBufferResource(device, intermediateSize);

	// データ転送コマンドを積む
	UpdateSubresources(commandList, texture, intermediateResource, 0, 0, UINT(subresources.size()), subresources.data());

	// テクスチャへの転送後は利用できるよう、D3D12_RESOURCE_STATE_COPY_DESTからD3D12_RESOURCE_STATE_GENERIC_READへResourceStateを変更する
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;

	commandList->ResourceBarrier(1, &barrier);

	return intermediateResource;
}

/// --- Z-Buffer ---
// DepthStencilTexture
ID3D12Resource* CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height) {
	// 生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width; // Textureの幅
	resourceDesc.Height = height; // Textureの高さ
	resourceDesc.MipLevels = 1; // mipmapの数
	resourceDesc.DepthOrArraySize = 1; // 奥行 or 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使う通知

	// 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValuue{};
	depthClearValuue.DepthStencil.Depth = 1.0f; // 1.0f（最大値）でクリア
	depthClearValuue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマット。Resourceと合わせる

	// Resourceの生成
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定、特になし
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込む状態にしておく
		&depthClearValuue, // Clear最適値
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));

	return resource;
}

#pragma endregion 関数の定義エリア

/// --- メイン処理 ---
// windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	// COMの初期化
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	assert(SUCCEEDED(hr));

	// 誰も捕捉しなかった場合に(Unhandled)、捕捉する関数を登録
	// main関数が始まってすぐに登録
	SetUnhandledExceptionFilter(ExportDump);

	// ログのディレクトリを用意
	std::filesystem::create_directory("logs");

	// 現在時刻を取得 (UTC時刻)
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	// ログファイルの名前にコンマ何秒はいらないので、削って秒にする
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	// 日本時間 (PCの設定時間) に変換
	std::chrono::zoned_time localTime{std::chrono::current_zone(), nowSeconds};
	// formatを使って年月日_時分秒の文字列に変換
	std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
	// 時刻を使ってファイル名を決定
	std::string logFilePath = std::string("logs/") + dateString + ".log";
	// ファイルを作って書き込み準備
	std::ofstream logStream(logFilePath);

	Log(logStream, "ぶっ飛ばすぜべいべ");

	// 出力ウィンドウへの文字出力
	OutputDebugStringA("Hello,DirectX!\n");

	WNDCLASS wc{};
	// ウィンドウプロシージャ
	wc.lpfnWndProc = WindowProc;
	// ウィンドウクラス名(何でもよい)
	wc.lpszClassName = L"CG2WindowClass";
	// インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);
	// カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	// ウィンドウクラスを登録する
	RegisterClass(&wc);

	// クライアント領域のサイズ
	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;

	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = {0, 0, kClientWidth, kClientHeight};

	// クライアント領域をもとに実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウの生成
	HWND hwnd = CreateWindow(
		wc.lpszClassName, // 利用するクラス名
		L"CG2", // タイトルバーの文字
		WS_OVERLAPPEDWINDOW, // ウィンドウスタイル
		CW_USEDEFAULT, // 表示X座標(Windowsに任せる)
		CW_USEDEFAULT, // 表示Y座標(windowsOSに任せる)
		wrc.right - wrc.left, // ウィンドウ横幅
		wrc.bottom - wrc.top, // ウィンドウ縦幅
		nullptr, // 親ウィンドウハンドル
		nullptr, // メニューハンドル
		wc.hInstance, // インスタンスハンドル
		nullptr // オプション
	);

	// ウィンドウを表示する
	ShowWindow(hwnd, SW_SHOW);

	/// --- DebugLayer ---

#ifdef _DEBUG
	ID3D12Debug1* debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		// デバッグレイヤーを有効化する
		debugController->EnableDebugLayer();
		// さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif // _DEBUG

	/// --- DXGI初期化 ---

	// DXGIファクトリーの生成
	IDXGIFactory7* dxgiFactory = nullptr;

	// HRESULTはWindowsケイのエラーコード、関数が成功したかどうかをSUCCEEDEDマクロで判定できる
	hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

	// 初期化の根本的な部分でエラーが出た場合はプログラムが間違っているか、どうにもできない場合はassertにしておく
	assert(SUCCEEDED(hr));

	// 使用するアダプタ用の変数、最初にnullptr
	IDXGIAdapter4* useAdapter = nullptr;

	// 良い順にアダプタを頼む
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
		// アダプターの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr)); // 取得できないのは一大事

		// ソフトウェアアダプタでなければ採用
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			// 採用したアダプタの情報をログに出力。wstringの方なので注意
			Log(logStream, std::format(L"Use Adapter:{}\n", adapterDesc.Description));
			break;
		}
		useAdapter = nullptr; // ソフトウェアアダプタの場合は見なかったことにする
	}

	// 適切なアダプタが見つからなかったので起動できない
	assert(useAdapter != nullptr);

	// --- D3D12Deviceの生成 ---
	ID3D12Device* device = nullptr;
	// 機能レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[]{
		D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
	};
	const char* featureLevelStrings[] = {"12.2", "12.1", "12.0"};
	// 高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		// 採用したアダプターでデバイスを生成
		hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));
		// 指定した機能レベルでデバイスが生成できたかを確認
		if (SUCCEEDED(hr)) {
			// 生成できたのでログ出力を行ってループを抜ける
			Log(logStream, std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
			break; // 生成できたらループを抜ける
		}
	}

	// デバイスの生成がうまくいかなかったので起動できない
	assert(device != nullptr);
	Log(logStream, "Complete create D3D12Device!!!\n"); // 初期化完了のログを出す

	// エラー・警告を実行時にプログラムを停止させる、deviceに対して行う
#ifdef _DEBUG
	ID3D12InfoQueue* infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		// やばいエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		// エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		// 警告時に止まる(ここをコメントアウトしたら全部の情報が出力される、詳細な情報をログに出力することができる)
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		// エラーと警告の抑制
		// 抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] = {
			// windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バクによるエラーメッセージ
			// https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

		// 抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[]
			= {D3D12_MESSAGE_SEVERITY_INFO};
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		// 指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);

		// 解放
		infoQueue->Release();
	}
#endif // _DEBUG

	/// --- CommandQueueの生成 ---
	ID3D12CommandQueue* commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
	//コマンドキューの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	/// --- CommandListの生成 ---
	// コマンドアロケータの生成
	ID3D12CommandAllocator* commandAllocator = nullptr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	// コマンドアロケータの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	// コマンドリストの生成
	ID3D12GraphicsCommandList* commandList = nullptr;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&commandList));
	// コマンドリストの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	/// --- SwapChainの生成 ---
	IDXGISwapChain4* swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = kClientWidth; // 画面の幅。ウィンドウのクライアント領域を同じものにしておく。
	swapChainDesc.Height = kClientHeight; // 画面の高さ。ウィンドウのクライアント領域を同じものにしておく。
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
	swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして利用する
	swapChainDesc.BufferCount = 2; // ダブルバッファ
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニターにうつしたら、中身を廃棄
	// コマンドキュー、ウィンドウハンドル、設定を渡して生成する。
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain));
	assert(SUCCEEDED(hr));

	/// --- DescriptorHeapの生成 ---
	// RTV用のヒープでディスクリプタの数は2。RTVはShader内で触るものではないので、ShaderVisibleはfalse
	ID3D12DescriptorHeap* rtvDescriptorHeap = CreateDesdcriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
	// SRV用のヒープでディスクリプタの数は128。SRVはShader内で触るものなので、ShaderVisibleはtrue
	ID3D12DescriptorHeap* srvDescriptorHeap = CreateDesdcriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

	/// --- DepthStencilView ---
	// DSV用のヒープでディスクリプタの数は1。DSVはShader内で触るものではないので、ShaderVisibleはfalse
	ID3D12DescriptorHeap* dsvDescriptorHeap = CreateDesdcriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	/// --- SwapChainからResourceを引っ張ってくる ---
	ID3D12Resource* swapChainResources[2] = {nullptr};
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));

	// --- RTVを作る ---
	// RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 出力結果をSRGBに変換して書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2dテクスチャとして書き込む
	// ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	// RTVを2つ作るのでディスクリプタを2つ用意
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	// まず1つ目を作る。1つ目は最初のところに作る。作る場所をこちらで指定してあげる必要がある。
	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0], &rtvDesc, rtvHandles[0]);
	// 2つ目のディスクリプタハンドルを得る(自力で)
	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	//2つ目を作る
	device->CreateRenderTargetView(swapChainResources[1], &rtvDesc, rtvHandles[1]);

	/// DXCの初期化
	// dxcCompilerを初期化
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	// 現時点でincludeはしないが、includeに対応するための設定を行っておく
	IDxcIncludeHandler* includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// DescriptorRange作成
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRange[0].NumDescriptors = 1; // 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR; // バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0; // レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// RootParameter作成
	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号0とバインド
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VertexShaderで使う
	rootParameters[1].Descriptor.ShaderRegister = 0;
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // Tableで利用する数
	descriptionRootSignature.pParameters = rootParameters; // ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters); // 配列の長さ

	// シリアライズしてバイナリにする
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Log(logStream, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	// バイナリを元に生成
	ID3D12RootSignature* rootSignature = nullptr;
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	// 全ての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 裏面(時計回り)を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// ShaderをCompileする
	IDxcBlob* vertexShaderBlob = CompileShader(L"Object3D.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, includeHandler, logStream);
	assert(vertexShaderBlob != nullptr);

	IDxcBlob* pixelShaderBlob = CompileShader(L"Object3D.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, includeHandler, logStream);
	assert(pixelShaderBlob != nullptr);

	/// --- PSOを生成する ---
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature; // RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
	graphicsPipelineStateDesc.VS = {vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize()}; // VertexShader
	graphicsPipelineStateDesc.PS = {pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize()}; // PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc; // BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState
	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	// 書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// 比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	// DepthStencilの設定、Depthを使いますよという意思表示
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトロポジ(形状)のタイプ、三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// 実際に生成
	ID3D12PipelineState* graphicsPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

	// WVP用のリソースを作る、Matrix4x4 １つ分のサイズを用意する
	ID3D12Resource* wvpResource = CreateBufferResource(device, sizeof(Matrix4x4));
	// データを書き込む
	Matrix4x4* wvpData = nullptr;
	// 書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	// 単位行列を書き込んでおく
	*wvpData = Matrix4x4::MakeIdentity4x4();

	/// VertexResourceを生成する
	// 頂点数の数
	// 最大頂点数
	const uint32_t kMaxVertexCount = 1024;
	ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(VertexData) * kMaxVertexCount);

	/// Material用のリソースを作る
	ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Vector4));
	// マテリアルにデータを書き込む
	Vector4* materialData = nullptr;
	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 色書き込み
	*materialData = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	/// Resourceのデータを書き込む
	// データを書き込む
	VertexData* vertexData = nullptr;
	// 書き込むためのアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	/*
	/// 三角形1個目
	// 左下
	vertexData[0].position = {-0.5f, -0.5f, 0.0f, 1.0f};
	vertexData[0].texcoord = {0.0f, 1.0f};
	// 上
	vertexData[1].position = {0.0f, 0.5f, 0.0f, 1.0f};
	vertexData[1].texcoord = {0.5f, 0.0f};
	// 右下
	vertexData[2].position = {0.5f, -0.5f, 0.0f, 1.0f};
	vertexData[2].texcoord = {1.0f, 1.0f};
	*/

	/*
	/// 三角形2個目
	// 左下
	vertexData[3].position = {-0.5f, -0.5f, 0.5f, 1.0f};
	vertexData[3].texcoord = {0.0f, 1.0f};
	// 上
	vertexData[4].position = {0.0f, 0.0f, 0.0f, 1.0f};
	vertexData[4].texcoord = {0.5f, 0.0f};
	// 右下
	vertexData[5].position = {0.5f, -0.5f, -0.5f, 1.0f};
	vertexData[5].texcoord = {1.0f, 1.0f};
	*/

	// 三角錐を構成する
	VertexData pyramidVertices[12] = {
		// 前面
		{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, // 左下
		{{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, // 頂点
		{{0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}}, // 右下

		// 右側面
		{{0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, // 右下
		{{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, // 頂点
		{{0.0f, -0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}}, // 奥

		// 左側面
		{{0.0f, -0.5f, 0.5f, 1.0f}, {0.0f, 1.0f}}, // 奥
		{{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, // 頂点
		{{-0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}}, // 左下

		// 底面
		{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, // 左前
		{{0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 0.0f}}, // 右前
		{{0.0f, -0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}}, // 奥
	};

	// データをGPUリソースへ書き込む(for文でコピー)
	for (uint32_t i = 0; i < 12; ++i) {
		vertexData[i] = pyramidVertices[i];
	}

	/// 頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	// リソースの戦闘のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	// 使用するリソースのサイズは最大頂点数(kMaxVertexCount)分のサイズにする
	vertexBufferView.SizeInBytes = sizeof(VertexData) * kMaxVertexCount;
	// 1頂点当たりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 初期値0でFenceを作る
	ID3D12Fence* fence = nullptr;
	uint64_t fenceValue = 0;
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));

	// FenceのSignalを持つためのイベントを作成する
	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);

	/// --- Textureの読み込みと転送 ---
	// Textureを読んで転送する
	DirectX::ScratchImage mipImages = LoadTexture("resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

	// VRAM上にテクスチャリソースを作成
	ID3D12Resource* textureResource = CreateTextureResource(device, metadata);

	// 深度ステンシルテクスチャリソースを作る
	ID3D12Resource* depthStencilResource = CreateDepthStencilTextureResource(device, kClientWidth, kClientHeight);

	// リソースの作成とコピーコマンドの記録
	ID3D12Resource* intermediateResource = UploadTextureData(textureResource, mipImages, device, commandList);

	// コマンドリストを確定して実行（キック）する
	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	ID3D12CommandList* commandLists[] = {commandList};
	commandQueue->ExecuteCommandLists(1, commandLists);

	// GPUの実行完了を待つ
	fenceValue++;
	commandQueue->Signal(fence, fenceValue);
	if (fence->GetCompletedValue() < fenceValue) {
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	// 転送が終わったのでソースは解放する
	intermediateResource->Release();

	// 次のフレームや初期化の続きのためにリセット
	hr = commandAllocator->Reset();
	assert(SUCCEEDED(hr));
	hr = commandList->Reset(commandAllocator, nullptr);
	assert(SUCCEEDED(hr));

	/// --- Texture用のSRV作成 ---
	// metaDataをもとにSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	// SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	//先頭はImGuiが使っているのでその次を使う
	textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// SRVの生成
	device->CreateShaderResourceView(textureResource, &srvDesc, textureSrvHandleCPU);

	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // Format、基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2dTexture
	// DSVHeapの先頭にDSVを作る
	device->CreateDepthStencilView(depthStencilResource, &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	/// --- ViewportとScissor ---
	// ビューポート
	D3D12_VIEWPORT viewport{};
	// クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = kClientWidth;
	viewport.Height = kClientHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	// シザー矩形
	D3D12_RECT scissorRect{};
	// ビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = kClientHeight;

	/// --- 文字列 ---
	// メッセージ構造体
	MSG msg{};

	/// ---変数の宣言---
	/// 三角形
	// Transformの変数を作る
	Transform transform = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	// カメラの回転
	bool isAutoRotate = false;

	// 描画モード
	int displayMode = 1;

	// 今フレーム描画する頂点数
	uint32_t drawVertexCount = 12;

	// モード1で使う
	float t1_Scale[3] = { 1.0f, 1.0f, 1.0f };
	float t1_Rotate[3] = { 0.0f, 0.0f, 0.0f };
	float t1_Translate[3] = { -0.2f, -0.2f, 0.0f };

	float t2_Scale[3] = { 1.0f, 1.0f, 1.0f };
	float t2_Rotate[3] = { 0.0f, 0.0f, 0.0f };
	float t2_Translate[3] = { 0.2f, 0.2f, 0.2f };

	// モード3で使う
	// 三角錐1個目
	float p1_Scale[3] = { 1.0f, 1.0f, 1.0f };
	float p1_Rotate[3] = { 0.0f, 0.0f, 0.0f };
	float p1_Translate[3] = { -0.3f, 0.0f, 0.0f };
	// 三角錐2個目
	float p2_Scale[3] = { 1.0f, 1.0f, 1.0f };
	float p2_Rotate[3] = { 0.0f, 0.0f, 0.0f };
	float p2_Translate[3] = { 0.3f, 0.0f, 0.0f };

	/// カメラ
	Transform cameraTransform{
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, -5.0f}
	};

	Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

	Matrix4x4 cameraMatrix = Matrix4x4::MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);

	Matrix4x4 viewMatrix = Matrix4x4::Inverse(cameraMatrix);

	Matrix4x4 projectionMatrix = Matrix4x4::MakePerspectiveFovMatrix(0.45f, float(kClientWidth) / float(kClientHeight), 0.1f, 100.0f);

	/// --- ImGuiの初期化 ---
#ifdef USE_IMGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(device, swapChainDesc.BufferCount, rtvDesc.Format, srvDescriptorHeap, srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Build();
#endif // USE_IMGUI

	/// --- メインループ ---
	// ウィンドウのxボタンが押されるまでループ
	while (msg.message != WM_QUIT) {
		//windowにメッセージが来てたら最優先で処理させる
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			/// --- ImGui先頭 ---
#ifdef USE_IMGUI
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
#endif // USE_IMGUI

			/// --- ゲームの処理 ---
			// 回転角を更新
			if (isAutoRotate) {
				transform.rotate.y += 0.003f;
				transform.rotate.x += 0.002f;
			}

			// --- モードに応じた頂点データの書き込み ---
			uint32_t drawVertexCount = 3; // デフォルト

			if (displayMode == 0) {
				// 0: 三角形1枚
				drawVertexCount = 3;
				VertexData triangleVertices[3] = {
					{{ 0.0f,  0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, // 上
					{{ 0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // 右下
					{{-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // 左下
				};
				for (uint32_t i = 0; i < drawVertexCount; ++i) {
					vertexData[i] = triangleVertices[i];
				}
			} else if (displayMode == 1) {
				// 1: 三角形2枚 (個別SRT)
				drawVertexCount = 6;
				// 三角形1個目
				vertexData[0].position = {-0.5f, -0.5f, 0.0f, 1.0f};
				vertexData[0].texcoord = {0.0f, 1.0f};
				vertexData[1].position = {0.0f, 0.5f, 0.0f, 1.0f};
				vertexData[1].texcoord = {0.5f, 0.0f};
				vertexData[2].position = {0.5f, -0.5f, 0.0f, 1.0f};
				vertexData[2].texcoord = {1.0f, 1.0f};

				// 三角形2個目
				vertexData[3].position = {-0.5f, -0.5f, 0.5f, 1.0f};
				vertexData[3].texcoord = {0.0f, 1.0f};
				vertexData[4].position = {0.0f, 0.0f, 0.0f, 1.0f};
				vertexData[4].texcoord = {0.5f, 0.0f};
				vertexData[5].position = {0.5f, -0.5f, -0.5f, 1.0f};
				vertexData[5].texcoord = {1.0f, 1.0f};

				// 1枚目の三角形の変形計算
				for (uint32_t i = 0; i < 3; ++i) {
					VertexData v = vertexData[i]; 

					float x = v.position.x * t1_Scale[0];
					float y = v.position.y * t1_Scale[1];
					float z = v.position.z * t1_Scale[2];

					// X軸回転
					float cosX = cosf(t1_Rotate[0]); float sinX = sinf(t1_Rotate[0]);
					float dy = y * cosX - z * sinX; float dz = y * sinX + z * cosX;
					y = dy; z = dz;

					// Y軸回転
					float cosY = cosf(t1_Rotate[1]); float sinY = sinf(t1_Rotate[1]);
					float dx = x * cosY + z * sinY; dz = -x * sinY + z * cosY;
					x = dx; z = dz;

					// Z軸回転
					float cosZ = cosf(t1_Rotate[2]); float sinZ = sinf(t1_Rotate[2]);
					dx = x * cosZ - y * sinZ; dy = x * sinZ + y * cosZ;
					x = dx; y = dy;

					// 平行移動
					v.position.x = x + t1_Translate[0];
					v.position.y = y + t1_Translate[1];
					v.position.z = z + t1_Translate[2];

					// 計算結果を上書き保存
					vertexData[i] = v;
				}

				// 2枚目の三角形の変形計算
				for (uint32_t i = 0; i < 3; ++i) {
					// インデックスを「i + 3」にする
					VertexData v = vertexData[i + 3]; 

					float x = v.position.x * t2_Scale[0];
					float y = v.position.y * t2_Scale[1];
					float z = v.position.z * t2_Scale[2];

					// X軸回転
					float cosX = cosf(t2_Rotate[0]); float sinX = sinf(t2_Rotate[0]);
					float dy = y * cosX - z * sinX; float dz = y * sinX + z * cosX;
					y = dy; z = dz;

					// Y軸回転
					float cosY = cosf(t2_Rotate[1]); float sinY = sinf(t2_Rotate[1]);
					float dx = x * cosY + z * sinY; dz = -x * sinY + z * cosY;
					x = dx; z = dz;

					// Z軸回転
					float cosZ = cosf(t2_Rotate[2]); float sinZ = sinf(t2_Rotate[2]);
					dx = x * cosZ - y * sinZ; dy = x * sinZ + y * cosZ;
					x = dx; y = dy;

					// 平行移動
					v.position.x = x + t2_Translate[0];
					v.position.y = y + t2_Translate[1];
					v.position.z = z + t2_Translate[2];

					// 計算結果を上書き保存
					vertexData[i + 3] = v;
				}
			} else if (displayMode == 2) {
				// 2: 三角錐1個
				drawVertexCount = 12;
				VertexData pyramidVertices[12] = {
					{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{ 0.0f,  0.5f,  0.0f, 1.0f}, {0.5f, 0.0f}}, {{ 0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}},
					{{ 0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{ 0.0f,  0.5f,  0.0f, 1.0f}, {1.0f, 1.0f}}, {{ 0.0f, -0.5f,  0.5f, 1.0f}, {1.0f, 1.0f}},
					{{ 0.0f, -0.5f,  0.5f, 1.0f}, {0.0f, 1.0f}}, {{ 0.0f,  0.5f,  0.0f, 1.0f}, {0.5f, 0.0f}}, {{-0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}},
					{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{ 0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 0.0f}}, {{ 0.0f, -0.5f,  0.5f, 1.0f}, {1.0f, 1.0f}},
				};
				for (uint32_t i = 0; i < drawVertexCount; ++i) {
					vertexData[i] = pyramidVertices[i];
				}
			} else if (displayMode == 3) {
				// 3: 三角錐2個 (個別SRT)
				drawVertexCount = 24;
				VertexData basePyramid[12] = {
					{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{ 0.0f,  0.5f,  0.0f, 1.0f}, {0.5f, 0.0f}}, {{ 0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}},
					{{ 0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{ 0.0f,  0.5f,  0.0f, 1.0f}, {0.5f, 0.0f}}, {{ 0.0f, -0.5f,  0.5f, 1.0f}, {1.0f, 1.0f}},
					{{ 0.0f, -0.5f,  0.5f, 1.0f}, {0.0f, 1.0f}}, {{ 0.0f,  0.5f,  0.0f, 1.0f}, {0.5f, 0.0f}}, {{-0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}},
					{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{ 0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 0.0f}}, {{ 0.0f, -0.5f,  0.5f, 1.0f}, {1.0f, 1.0f}},
				};

				// 1個目の三角錐の変形計算
				for (uint32_t i = 0; i < 12; ++i) {
					VertexData v = basePyramid[i];
					float x = v.position.x * p1_Scale[0];
					float y = v.position.y * p1_Scale[1];
					float z = v.position.z * p1_Scale[2];
					// X軸回転
					float cosX = cosf(p1_Rotate[0]); float sinX = sinf(p1_Rotate[0]);
					float dy = y * cosX - z * sinX; float dz = y * sinX + z * cosX;
					y = dy; z = dz;
					// Y軸回転
					float cosY = cosf(p1_Rotate[1]); float sinY = sinf(p1_Rotate[1]);
					float dx = x * cosY + z * sinY; dz = -x * sinY + z * cosY;
					x = dx; z = dz;
					// Z軸回転
					float cosZ = cosf(p1_Rotate[2]); float sinZ = sinf(p1_Rotate[2]);
					dx = x * cosZ - y * sinZ; dy = x * sinZ + y * cosZ;
					x = dx; y = dy;
					// 平行移動
					v.position.x = x + p1_Translate[0];
					v.position.y = y + p1_Translate[1];
					v.position.z = z + p1_Translate[2];
					vertexData[i] = v;
				}

				// 2個目の三角錐の変形計算
				for (uint32_t i = 0; i < 12; ++i) {
					VertexData v = basePyramid[i];
					float x = v.position.x * p2_Scale[0];
					float y = v.position.y * p2_Scale[1];
					float z = v.position.z * p2_Scale[2];
					// X軸回転
					float cosX = cosf(p2_Rotate[0]); float sinX = sinf(p2_Rotate[0]);
					float dy = y * cosX - z * sinX; float dz = y * sinX + z * cosX;
					y = dy; z = dz;
					// Y軸回転
					float cosY = cosf(p2_Rotate[1]); float sinY = sinf(p2_Rotate[1]);
					float dx = x * cosY + z * sinY; dz = -x * sinY + z * cosY;
					x = dx; z = dz;
					// Z軸回転
					float cosZ = cosf(p2_Rotate[2]); float sinZ = sinf(p2_Rotate[2]);
					dx = x * cosZ - y * sinZ; dy = x * sinZ + y * cosZ;
					x = dx; y = dy;
					// 平行移動
					v.position.x = x + p2_Translate[0];
					v.position.y = y + p2_Translate[1];
					v.position.z = z + p2_Translate[2];
					vertexData[i + 12] = v;
				}
			} else if (displayMode == 4) {
				// 4: 演出モード
				drawVertexCount = 3;
				VertexData productionVertices[3] = {
					{{ 0.0f,  0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, // 上
					{{ 0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // 右下
					{{-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // 左下
				};
				for (uint32_t i = 0; i < drawVertexCount; ++i) {
					vertexData[i] = productionVertices[i];
				}
			}

			/// --- 行列の計算 ---
			// ワールド行列の更新
			worldMatrix = Matrix4x4::MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

			// カメラ行列の更新
			cameraMatrix = Matrix4x4::MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);

			// ビュー行列の更新
			viewMatrix = Matrix4x4::Inverse(cameraMatrix);

			// プロジェクション行列の更新
			projectionMatrix = Matrix4x4::MakePerspectiveFovMatrix(0.45f, float(kClientWidth) / float(kClientHeight), 0.1f, 100.0f);

			// ワールド、ビュー、プロジェクションを掛け合わせる
			Matrix4x4 worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, Matrix4x4::Multiply(viewMatrix, projectionMatrix));

			// wvp行列をGPUに送る
			*wvpData = worldViewProjectionMatrix;

			/// --- ImGui中身 ---
#ifdef USE_IMGUI
			// 開発用UIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
			ImGui::ShowDemoWindow();

			ImGui::Begin("Window");

			/// --- 色変えれます ---
			ImGui::Text("Color Control");
			ImGui::ColorEdit4("Material Color", &materialData->x);

			// 区切り線
			ImGui::Separator();

			/// --- モード切り替えを切り替えだドン ---
			const char* modes[] = { 
				"0: Single Triangle", 
				"1: Double Triangles", 
				"2: Single Pyramid", 
				"3: Double Pyramids (Individual SRT)", 
				"4: Production Mode" 
			};
			ImGui::Combo("Display Mode", &displayMode, modes, IM_ARRAYSIZE(modes));

			// モード1(三角形2枚)の個別SRTスライダー
			if (displayMode == 1) {
				ImGui::Text("[Triangle 1]");
				ImGui::SliderFloat3("T1 Scale", t1_Scale, 0.1f, 5.0f);
				ImGui::SliderFloat3("T1 Rotation", t1_Rotate, -3.1415f, 3.1415f);
				ImGui::SliderFloat3("T1 Position", t1_Translate, -3.0f, 3.0f);

				ImGui::Separator();

				ImGui::Text("[Triangle 2]");
				ImGui::SliderFloat3("T2 Scale", t2_Scale, 0.1f, 5.0f);
				ImGui::SliderFloat3("T2 Rotation", t2_Rotate, -3.1415f, 3.1415f);
				ImGui::SliderFloat3("T2 Position", t2_Translate, -3.0f, 3.0f);

				ImGui::Separator();
			}

			// モード3(三角錐2個)の個別SRTスライダー
			if (displayMode == 3) {
				ImGui::Text("[Pyramid 1]");
				ImGui::SliderFloat3("P1 Scale", p1_Scale, 0.1f, 5.0f);
				ImGui::SliderFloat3("P1 Rotation", p1_Rotate, -3.1415f, 3.1415f);
				ImGui::SliderFloat3("P1 Position", p1_Translate, -3.0f, 3.0f);

				ImGui::Separator();

				ImGui::Text("[Pyramid 2]");
				ImGui::SliderFloat3("P2 Scale", p2_Scale, 0.1f, 5.0f);
				ImGui::SliderFloat3("P2 Rotation", p2_Rotate, -3.1415f, 3.1415f);
				ImGui::SliderFloat3("P2 Position", p2_Translate, -3.0f, 3.0f);

				ImGui::Separator();
			}

			// 区切り線
			ImGui::Separator();

			/// --- 自動で回転かと座標変えれます ---
			ImGui::Text("Pyramid Control");

			// 1_拡縮の変更(XYZ)
			ImGui::SliderFloat3("Scale", &transform.scale.x, 0.1f, 10.0f);

			// 2_上下左右・奥への位置移動(XYZ)
			ImGui::SliderFloat3("Position", &transform.translate.x, -5.0f, 5.0f);

			// 3_自動回転の切り替えボタン
			// ボタンを押すたびにON/OFFが切り替わり、OFFになった瞬間に回転を初期値(0)にリセット
			if (ImGui::Button(isAutoRotate ? "Stop & Reset" : "Start Auto Rotate")) {
				isAutoRotate = !isAutoRotate;
				if (!isAutoRotate) {
					transform.rotate = {0.0f, 0.0f, 0.0f}; // 回転を初期値に戻す
				}
			}

			// 4_フラグの状態確認(0か1かで表示)
			ImGui::Text("Auto Rotate Flag: %d", isAutoRotate ? 1 : 0);

			// 5_各軸の回転(XYZ)
			// 自動回転がOFFのときだけ手動でいじれるようにしてONのときは現在の回転角を表示する
			if (!isAutoRotate) {
				ImGui::SliderFloat3("Rotation", &transform.rotate.x, -3.1415f, 3.1415f);
			} else {
				ImGui::Text("Rotation (Auto): X:%.2f, Y:%.2f, Z:%.2f", transform.rotate.x, transform.rotate.y, transform.rotate.z);
			}

			// 6_すべてのパラメータをリセット (SRTと自動回転を初期値に戻す)
			if (ImGui::Button("Reset All")) {
				// グローバルSRTと自動回転のリセット
				transform.scale = {1.0f, 1.0f, 1.0f};
				transform.rotate = {0.0f, 0.0f, 0.0f};
				transform.translate = {0.0f, 0.0f, 0.0f};
				isAutoRotate = false;

				// 現在のモードを維持したまま各パラメータをリセット
				for (int i = 0; i < 3; ++i) {
					t1_Scale[i] = 1.0f;  t1_Rotate[i] = 0.0f;
					t2_Scale[i] = 1.0f;  t2_Rotate[i] = 0.0f;
					p1_Scale[i] = 1.0f;  p1_Rotate[i] = 0.0f;
					p2_Scale[i] = 1.0f;  p2_Rotate[i] = 0.0f;
				}
				// モード1の初期位置
				t1_Translate[0] = -0.2f; t1_Translate[1] = -0.2f; t1_Translate[2] = 0.0f;
				t2_Translate[0] = 0.2f;  t2_Translate[1] = 0.2f;  t2_Translate[2] = 0.2f;

				// モード3の初期位置
				p1_Translate[0] = -0.3f; p1_Translate[1] = 0.0f;  p1_Translate[2] = 0.0f;
				p2_Translate[0] = 0.3f;  p2_Translate[1] = 0.0f;  p2_Translate[2] = 0.0f;
			}

			// 区切り線
			ImGui::Separator();

			ImGui::End();
#endif // USE_IMGUI

			/// --- ImGui終わり ---
#ifdef USE_IMGUI
			// 内部コマンドを生成する
			ImGui::Render();
#endif // USE_IMGUI

#pragma region
			/// --- コマンドを積む ---
			// これから書き込むバックバッファのインデックスを取得
			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

			// TransitionBarrierの設定
			D3D12_RESOURCE_BARRIER barrier{};
			// 今回のバリアはTransition
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			// Noneにしておく
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			// バリアを張る対象のリソース。現在のバックバッファに対して行う
			barrier.Transition.pResource = swapChainResources[backBufferIndex];
			// 遷移前(現在)のResourceState
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			// 遷移後のResourceState
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			// TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);

			// 描画先のRTVとDSVを設定する
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
			// 指定した色で画面全体をクリアする
			float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f}; // 青っぽい色。RGBAの順
			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			// 描画用のDescriptorHeapの設定
			ID3D12DescriptorHeap* descriptorHeaps[] = {srvDescriptorHeap};
			commandList->SetDescriptorHeaps(1, descriptorHeaps);

			// 描画の設定(ドローコール)
			// ViewportとScissorRectの設定
			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &scissorRect);
			// RootSignatureの設定
			commandList->SetGraphicsRootSignature(rootSignature);
			// PSOの設定
			commandList->SetPipelineState(graphicsPipelineState);
			// 頂点バッファビューの設定
			commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
			// 形状を設定
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			// マテリアルCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			// wvp用のBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
			// SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である
			commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
			// 描画(DrawCall/ドローコール)、3頂点で1つのインスタンス
			commandList->DrawInstanced(drawVertexCount, 1, 0, 0);

			// 画面表示できるようにする
			// 今回はRenderTargetからPresentにする
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

			// 実際のcommandListのImGuiの描画コマンドを積む
#ifdef USE_IMGUI
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
			// TransitionBarrerを張る
			commandList->ResourceBarrier(1, &barrier);
#endif // USE_IMGUI

			// コマンドリストの内容を確定させる。すべてのコマンドを積んでからCloseすること
			hr = commandList->Close();
			assert(SUCCEEDED(hr));

			/// --- キックする ---
			// GPUにコマンドリストの実行を行わせる
			ID3D12CommandList* commandLists[] = {commandList};
			commandQueue->ExecuteCommandLists(1, commandLists);
			// GPUとOSに画面の交換を行うよう通知する
			swapChain->Present(1, 0);

			// GPUにSignalを送る
			// Fenceの値を更新
			fenceValue++;
			// GPU画がここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
			commandQueue->Signal(fence, fenceValue);
			// Fenceの値が指定したSignal値にたどり着いているか確認する
			// GetCompletedValueの初期値はFence作成時に渡した初期値
			if (fence->GetCompletedValue() < fenceValue) {
				// 指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する。
				fence->SetEventOnCompletion(fenceValue, fenceEvent);
				// イベントを待つ
				WaitForSingleObject(fenceEvent, INFINITE);
			}

			// 次のフレーム用のコマンドリストを準備
			hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr));
			hr = commandList->Reset(commandAllocator, nullptr);
			assert(SUCCEEDED(hr));
#pragma endregion コマンドを積む処理
		}
	} // whileの終わり

	  /// --- ImGui終了処理 ---
#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif // USE_IMGUI

	/// --- 解放処理 ---
	// 1_各種バッファ・テクスチャ・リソース（すべてdeviceより前）
	vertexResource->Release();
	wvpResource->Release();
	materialResource->Release();
	textureResource->Release();
	depthStencilResource->Release();

	// 2_シェーダ・Blob関連（これらもdeviceより前）
	if (signatureBlob) {
		signatureBlob->Release();
	}
	if (errorBlob) {
		errorBlob->Release();
	}
	vertexShaderBlob->Release();
	pixelShaderBlob->Release();

	// 3_パイプライン・コマンド関連
	graphicsPipelineState->Release();
	rootSignature->Release();
	commandList->Release();
	commandAllocator->Release();
	commandQueue->Release();

	// 4. ディスクリプタヒープ
	rtvDescriptorHeap->Release();
	srvDescriptorHeap->Release();
	dsvDescriptorHeap->Release();

	// 5. スワップチェーンとバックバッファリソース
	swapChainResources[0]->Release();
	swapChainResources[1]->Release();
	swapChain->Release();

	// 6. フェンスとイベント
	CloseHandle(fenceEvent);
	fence->Release();

	// 7. DXC関連のツール
	includeHandler->Release();
	dxcCompiler->Release();
	dxcUtils->Release();
#ifdef _DEBUG
	debugController->Release();
#endif // _DEBUG

	// 8_すべての依存リソースが消えたので解放
	device->Release();
	useAdapter->Release();
	dxgiFactory->Release();

	// DX12リソースがなくなった後にウィンドウを閉じる
	CloseWindow(hwnd);

	/// --- ReportLiveObjects ---
	// リソースリークチェック
	IDXGIDebug1* debug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		debug->Release();
	}

	// COMの終了処理
	CoUninitialize();

	return 0;
}
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
#include "BufferResource.h"
#include "Collision.h"
#include "CommandContext.h"
#include "DepthStencilView.h"
#include "DescriptorHeap.h"
#include "Logger.h"
#include "ParticleSystem.h"
#include "PipelineState.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "VertexBuffer.h"

#include <filesystem> // フォルダとファイルを列挙するため
#include <string>     // ファイル名をstd::stringで扱うため
#include <system_error> // フォルダ列挙エラーを安全に受け取るため

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

///// ----- ImGuiパネル ----- /////
#ifdef USE_IMGUI
// Hierarchyで現在選択されている項目を表す
enum class SelectedObject {
	None,
	Pyramid,
	Sprite,
	ParticleSystem
};

// 選択結果をPropertiesへ渡せるように参照で受け取る
void DrawHierarchy(SelectedObject& selectedObject) {
	ImGui::Begin("Hierarchy");

	if (ImGui::Selectable(
		"Pyramid",
		selectedObject == SelectedObject::Pyramid)) {
		selectedObject = SelectedObject::Pyramid;
	}

	if (ImGui::Selectable(
		"Sprite",
		selectedObject == SelectedObject::Sprite)) {
		selectedObject = SelectedObject::Sprite;
	}

	if (ImGui::Selectable(
		"ParticleSystem",
		selectedObject == SelectedObject::ParticleSystem)) {
		selectedObject = SelectedObject::ParticleSystem;
	}

	ImGui::End();
}

// Scene用レンダーテクスチャを作るまでは仮表示にする
void DrawScene() {
	ImGui::Begin("Scene");
	ImGui::Text("Scene view is not implemented yet.");
	ImGui::End();
}

// 選択オブジェクトとの接続は後から実装する
void DrawProperties() {
	ImGui::Begin("Properties");
	ImGui::Text("Select an object.");
	ImGui::End();
}

// 描画統計を表示する
void DrawStatistics() {
	ImGui::Begin("Statistics");
	ImGui::Text("Statistics");
	ImGui::End();
}

// 指定されたフォルダの中身だけを再帰的に表示する
void DrawDirectoryTree(const std::filesystem::path& directory) {
	std::error_code error;

	for (const auto& entry :
		std::filesystem::directory_iterator(directory, error)) {

		if (error) {
			ImGui::Text("Failed to read directory.");
			return;
		}

		const std::string name = entry.path().filename().string();

		if (entry.is_directory()) {
			// フォルダをツリーとして開閉可能にする
			if (ImGui::TreeNode(name.c_str())) {
				DrawDirectoryTree(entry.path());
				ImGui::TreePop();
			}
		} else {
			// ファイルを選択可能な項目として表示する
			ImGui::Selectable(name.c_str());
		}
	}
}

// Content Browserウィンドウは毎フレーム1回だけ作る
void DrawContentBrowser() {
	ImGui::Begin("Content Browser");

	const std::filesystem::path resourceDirectory = "resources";

	if (std::filesystem::exists(resourceDirectory)) {
		DrawDirectoryTree(resourceDirectory);
	} else {
		ImGui::Text("resources folder was not found.");
	}

	ImGui::End();
}

#endif

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

	///// ----- CommandContext ----- /////

	/// --- 初期化 ---
	// コマンドの記録と実行を管理する
	CommandContext commandContext;
	commandContext.Initialize(device);

	// 描画コマンドを積むCommandListを取得する
	ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList();

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
	hr = dxgiFactory->CreateSwapChainForHwnd(commandContext.GetCommandQueue(), hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain));
	assert(SUCCEEDED(hr));

	/// --- DescriptorHeapの生成 ---
	// RTV用のヒープでディスクリプタの数は2。RTVはShader内で触るものではないので、ShaderVisibleはfalse
	DescriptorHeap rtvDescriptorHeap;
	rtvDescriptorHeap.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
	// SRV用のヒープでディスクリプタの数は128。SRVはShader内で触るものなので、ShaderVisibleはtrue
	DescriptorHeap srvDescriptorHeap;
	srvDescriptorHeap.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

	/// --- DepthStencilView ---
	// DSV用のヒープでディスクリプタの数は1。DSVはShader内で触るものではないので、ShaderVisibleはfalse
	DescriptorHeap dsvDescriptorHeap;
	dsvDescriptorHeap.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

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
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap.GetCPUHandleStart();
	// RTVを2つ作るのでディスクリプタを2つ用意
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	// まず1つ目を作る。1つ目は最初のところに作る。作る場所をこちらで指定してあげる必要がある。
	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0], &rtvDesc, rtvHandles[0]);
	// 2つ目のディスクリプタハンドルを得る(自力で)
	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	//2つ目を作る
	device->CreateRenderTargetView(swapChainResources[1], &rtvDesc, rtvHandles[1]);

	///// ----- PSO(Pipeline State Object) ----- /////

	/// --- 初期化 ---
	// RootSignatureと各設定をまとめてPSOを作成する
	PipelineState pipelineState;
	pipelineState.Initialize(device, logStream);

	// WVP用のリソースを作る、Matrix4x4 １つ分のサイズを用意する
	ID3D12Resource* wvpResource = BufferResource::Create(device, sizeof(Matrix4x4));
	// データを書き込む
	Matrix4x4* wvpData = nullptr;
	// 書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	// 単位行列を書き込んでおく
	*wvpData = Matrix4x4::MakeIdentity4x4();

	///// ----- VertexBuffer ----- /////

	/// --- 初期化 ---
	// VertexResourceを生成する
	// 頂点数の数
	// 最大頂点数
	const uint32_t kMaxVertexCount = 1024;
	VertexBuffer vertexBuffer;
	vertexBuffer.Initialize(device, kMaxVertexCount);

	///// ----- Sprite ----- /////

	/// --- 初期化 ---
	// Sprite専用のVertexBuffer、Material、WVPを作成
	Sprite sprite;
	sprite.Initialize(device, kClientWidth, kClientHeight, 640.0f, 360.0f);

	// 三角形とは別にSpriteのTextureを選択する
	int spriteTextureMode = 1;

	/// Material用のリソースを作る
	ID3D12Resource* materialResource = BufferResource::Create(device, sizeof(Vector4));
	// マテリアルにデータを書き込む
	Vector4* materialData = nullptr;
	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 色書き込み
	*materialData = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	/// Resourceのデータを書き込む
	// データを書き込む
	VertexData* vertexData = vertexBuffer.GetData();
	// 書き込むためのアドレスを取得
	// VertexBufferの初期化時にMapしたアドレスを使用する

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

	///// ----- TextureManager ----- /////

	/// --- 初期化 ---
	// テクスチャの切り替え用
	int textureMode = 0;

	// Textureを読み込み、GPUへの転送とSRVの作成を行う
	TextureManager textureManager;
	textureManager.Initialize(device, commandList, srvDescriptorHeap);

	///// ----- DSV(Depth Stencil View) ----- /////

	/// --- 初期化 ---
	// 深度ステンシルテクスチャリソースとDSVを作る
	DepthStencilView depthStencilView;
	depthStencilView.Initialize(device, dsvDescriptorHeap, kClientWidth, kClientHeight);

	/// --- Texture転送コマンドの実行 ---
	// コマンドを実行してGPUの完了を待つ
	commandContext.ExecuteAndWait();

	// 転送が終わったのでソースは解放する
	// GPUへの転送が終わったので転送用リソースを解放する
	textureManager.ReleaseIntermediateResources();

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

	// モード1で使う
	float t1_Scale[3] = {1.0f, 1.0f, 1.0f};
	float t1_Rotate[3] = {0.0f, 0.0f, 0.0f};
	float t1_Translate[3] = {0.0f, 0.0f, 0.0f};

	float t2_Scale[3] = {1.0f, 1.0f, 1.0f};
	float t2_Rotate[3] = {0.0f, 0.0f, 0.0f};
	float t2_Translate[3] = {0.0f, 0.0f, 0.0f};

	// モード3で使う
	// 三角錐1個目
	float p1_Scale[3] = {1.0f, 1.0f, 1.0f};
	float p1_Rotate[3] = {0.0f, 0.0f, 0.0f};
	float p1_Translate[3] = {-0.3f, 0.0f, 0.0f};
	// 三角錐2個目
	float p2_Scale[3] = {1.0f, 1.0f, 1.0f};
	float p2_Rotate[3] = {0.0f, 0.0f, 0.0f};
	float p2_Translate[3] = {0.3f, 0.0f, 0.0f};

	///// ----- ParticleSystem ----- /////

	/// --- 初期化 ---
	// 演出モード4で使用する範囲と最大数を設定する
	ParticleSystem particleSystem;
	particleSystem.Initialize(device, -0.9f, 0.9f, 50);

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

	///// ----- ImGuiの初期化 ----- /////
#ifdef USE_IMGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();

	// Docking対応版へ更新した後に有効化する
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);

	// Dear ImGuiのDirectX 12初期化情報をまとめる
	ImGui_ImplDX12_InitInfo initInfo{};
	initInfo.Device = device;
	initInfo.CommandQueue = commandContext.GetCommandQueue();
	initInfo.NumFramesInFlight = swapChainDesc.BufferCount;
	initInfo.RTVFormat = rtvDesc.Format;
	initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
	initInfo.SrvDescriptorHeap = srvDescriptorHeap.Get();

	// 動的なSRV管理を実装するまで、従来どおり先頭ディスクリプタを使用する
	initInfo.LegacySingleSrvCpuDescriptor =
		srvDescriptorHeap.GetCPUHandleStart();
	initInfo.LegacySingleSrvGpuDescriptor =
		srvDescriptorHeap.GetGPUHandleStart();

	// 新しいDirectX 12バックエンド初期化形式を使用する
	ImGui_ImplDX12_Init(&initInfo);

	io.Fonts->Build();

	SelectedObject selectedObject = SelectedObject::None;
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

			// 画面全体を各エディターパネルの配置領域として使用する
			ImGui::DockSpaceOverViewport();

			DrawHierarchy(selectedObject);
			DrawScene();
			DrawProperties();
			DrawContentBrowser();
			DrawStatistics();
#endif // USE_IMGUI

			/// --- ゲームの処理 ---
			// 回転角を更新
			if (isAutoRotate) {
				transform.rotate.y += 0.005f;
				//transform.rotate.x += 0.002f;
			}

			// --- モードに応じた頂点データの書き込み ---
			// ImGuiで切り替えても、現在のフレームは同じモードで更新と描画を行う
			const int renderingMode = displayMode;
			uint32_t drawVertexCount = 3; // デフォルト

			if (renderingMode == 0) {
				// 0: 三角形1枚
				drawVertexCount = 3;
				VertexData triangleVertices[3] = {
					{{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, // 上
					{{0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // 右下
					{{-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // 左下
				};
				for (uint32_t i = 0; i < drawVertexCount; ++i) {
					vertexData[i] = triangleVertices[i];
				}
			} else if (renderingMode == 1) {
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
			} else if (renderingMode == 2) {
				// 2: 三角錐1個
				drawVertexCount = 12;
				VertexData pyramidVertices[12] = {
					{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, {{0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}},
					{{0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.0f, 0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}}, {{0.0f, -0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}},
					{{0.0f, -0.5f, 0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, {{-0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}},
					{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 0.0f}}, {{0.0f, -0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}},
				};
				for (uint32_t i = 0; i < drawVertexCount; ++i) {
					vertexData[i] = pyramidVertices[i];
				}
			} else if (renderingMode == 3) {
				// 3: 三角錐2個 (個別SRT)
				drawVertexCount = 24;
				VertexData basePyramid[12] = {
					{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, {{0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}},
					{{0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, {{0.0f, -0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}},
					{{0.0f, -0.5f, 0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, {{-0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}},
					{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 0.0f}}, {{0.0f, -0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}},
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
			} else if (renderingMode == 4) {
				// 4: 演出モード

				// Particle用の三角形をVertexBufferへ書き込む
				particleSystem.WriteTriangleVertices(vertexData);

				// 移動、回転、壁反射、Particleの追加を行う
				particleSystem.Update();
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

			///// ----- ImGui中身 ----- /////
#ifdef USE_IMGUI
			// 開発用UIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
			ImGui::ShowDemoWindow();

			ImGui::Begin("Window");

			// Hierarchyで選択されたオブジェクトの設定をPropertiesへ表示する
			ImGui::Begin("Properties");

			if (selectedObject == SelectedObject::Pyramid) {
				ImGui::Text("Transform");

				// 選択中のオブジェクトだけを編集できるようにする
				ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f);
				ImGui::DragFloat3("Rotation", &transform.rotate.x, 0.01f);
				ImGui::DragFloat3("Position", &transform.translate.x, 0.01f);

				ImGui::Separator();

				// 既存のマテリアル設定をPropertiesへ移す
				ImGui::ColorEdit4("Material Color", &materialData->x);
			}

			ImGui::End();

			/// --- 色変えれます ---
			ImGui::Text("Color Control");
			ImGui::ColorEdit4("Material Color", &materialData->x);

			// 区切り線
			ImGui::Separator();

			/// --- 画像変えれます ---
			// テクスチャ切り替え
			const char* textureModes[] = {
				"0 : No Texture (White)",
				"1 : UV Checker",
				"2 : Genbaneko"
			};

			ImGui::Combo(
				"Texture Mode",
				&textureMode,
				textureModes,
				IM_ARRAYSIZE(textureModes)
			);

			// 区切り線
			ImGui::Separator();

			///// ----- Sprite ----- /////

			/// --- Texture ---
			// 三角形とは別にSpriteのTextureを切り替える
			ImGui::Text("Sprite Control");
			ImGui::Combo(
				"Sprite Texture Mode",
				&spriteTextureMode,
				textureModes,
				IM_ARRAYSIZE(textureModes)
			);

			/// --- 色 ---
			// Sprite専用のMaterial Colorを変更する
			ImGui::ColorEdit4("Sprite Material Color", &sprite.GetColor().x);

			/// --- SRT ---
			// Sprite専用のScale、Rotate、Translateを変更する
			Transform& spriteTransform = sprite.GetTransform();
			ImGui::DragFloat3("Sprite Scale", &spriteTransform.scale.x, 0.01f);
			ImGui::DragFloat3("Sprite Rotate", &spriteTransform.rotate.x, 0.01f);
			ImGui::DragFloat3("Sprite Translate", &spriteTransform.translate.x, 1.0f);

			// Spriteだけを初期状態へ戻す
			if (ImGui::Button("Reset Sprite")) {
				sprite.Reset();
			}

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

				// 全Particleを削除して最初の1個を生成する
				particleSystem.Reset();
			}

			// 区切り線
			ImGui::Separator();

			if (displayMode == 4) {
				ImGui::Text("Triangle Count : %d",
					static_cast<int>(particleSystem.GetParticleCount()));
				ImGui::Text("Total Vertex Count : %d",
					static_cast<int>(particleSystem.GetTotalVertexCount()));
			}

			const ParticleSystem::Particle* firstParticle = particleSystem.GetFirstParticle();
			if (firstParticle != nullptr) {
				ImGui::Text(
					"Pos X: %.3f Y: %.3f",
					firstParticle->position.x,
					firstParticle->position.y
				);
			}

			ImGui::End();
#endif // USE_IMGUI

			// ImGuiで変更されたSpriteのSRTからWVPを更新する
			sprite.Update();

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
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthStencilView.GetHandle();
			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
			// 指定した色で画面全体をクリアする
			float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f}; // 青っぽい色。RGBAの順
			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			// 描画用のDescriptorHeapの設定
			ID3D12DescriptorHeap* descriptorHeaps[] = {srvDescriptorHeap.Get()};
			commandList->SetDescriptorHeaps(1, descriptorHeaps);

			// 描画の設定(ドローコール)
			// ViewportとScissorRectの設定
			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &scissorRect);
			// RootSignatureの設定
			commandList->SetGraphicsRootSignature(pipelineState.GetRootSignature());
			// PSOの設定
			commandList->SetPipelineState(pipelineState.GetPipelineState());
			// 頂点バッファビューの設定
			const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView = vertexBuffer.GetView();
			commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
			// 形状を設定
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			// マテリアルCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			// wvp用のBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());

			// 画像を指定
			D3D12_GPU_DESCRIPTOR_HANDLE currentTextureHandle =
				textureManager.GetSrvHandle(static_cast<uint32_t>(textureMode));

			// 選択されたテクスチャをシェーダーへ渡す
			commandList->SetGraphicsRootDescriptorTable(
				2,
				currentTextureHandle
			);

			// 1_モード4は全三角形描画
			if (renderingMode == 4) {
				particleSystem.Draw(
					commandList,
					viewMatrix,
					projectionMatrix
				);
			}

			// 2_それ以外のモード
			else {
				*wvpData = worldViewProjectionMatrix;

				commandList->DrawInstanced(
					drawVertexCount,
					1,
					0,
					0
				);
			}

			///// ----- Sprite描画 ----- /////

			/// --- Texture ---
			// 三角形とは別に選択されたTextureを取得する
			const D3D12_GPU_DESCRIPTOR_HANDLE spriteTextureHandle =
				textureManager.GetSrvHandle(static_cast<uint32_t>(spriteTextureMode));

			/// --- 描画 ---
			// 3Dの後に描画してSpriteを最前面へ表示する
			sprite.Draw(commandList, spriteTextureHandle);

			// 画面表示できるようにする
			// 今回はRenderTargetからPresentにする
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

			// 実際のcommandListのImGuiの描画コマンドを積む
#ifdef USE_IMGUI
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif // USE_IMGUI
			// TransitionBarrerを張る
			commandList->ResourceBarrier(1, &barrier);

			///// ----- CommandContext ----- /////

			/// --- コマンドの実行 ---
			// コマンドを実行して画面を表示する
			commandContext.ExecuteAndPresent(swapChain);
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
	// 1_各種バッファ・テクスチャ・リソース(すべてdeviceより前)
	vertexBuffer.Finalize();
	sprite.Finalize();
	particleSystem.Finalize();
	wvpResource->Release();
	materialResource->Release();
	// 読み込んだテクスチャをすべて解放する
	textureManager.Finalize();
	depthStencilView.Finalize();

	// 2_PSOとコマンド関連は各クラスが解放する
	pipelineState.Finalize();
	commandContext.Finalize();

	// 4_ディスクリプタヒープ
	rtvDescriptorHeap.Finalize();
	srvDescriptorHeap.Finalize();
	dsvDescriptorHeap.Finalize();

	// 5_スワップチェーンとバックバッファリソース
	swapChainResources[0]->Release();
	swapChainResources[1]->Release();
	swapChain->Release();

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

#include "CommandContext.h"

#include <cassert>

///// ----- CommandContext ----- /////

CommandContext::~CommandContext() {
	Finalize();
}

/// --- 初期化 ---
// コマンドの記録と実行に必要なリソースを作成
void CommandContext::Initialize(ID3D12Device* device) {
	/// --- CommandQueue ---
	// CommandQueueの生成
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	HRESULT hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(commandQueue_.GetAddressOf()));
	// コマンドキューの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	/// --- CommandAllocator ---
	// コマンドアロケータの生成
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator_.GetAddressOf()));
	// コマンドアロケータの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	/// --- CommandList ---
	// コマンドリストの生成
	hr = device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocator_.Get(),
		nullptr,
		IID_PPV_ARGS(commandList_.GetAddressOf())
	);
	// コマンドリストの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	/// --- Fence ---
	// 初期値0でFenceを作る
	hr = device->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf()));
	assert(SUCCEEDED(hr));

	// FenceのSignalを持つためのイベントを作成する
	fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent_ != nullptr);
}

/// --- コマンドの実行 ---
// コマンドを実行してGPUの完了を待つ
void CommandContext::ExecuteAndWait() {
	// コマンドリストを確定して実行(キック)する
	HRESULT hr = commandList_->Close();
	assert(SUCCEEDED(hr));

	// GPUにコマンドリストの実行を行わせる
	ID3D12CommandList* commandLists[] = {commandList_.Get()};
	commandQueue_->ExecuteCommandLists(1, commandLists);

	// GPUの実行完了を待つ
	WaitForGPU();

	// 次のフレームや初期化の続きのためにリセット
	Reset();
}

// コマンドを実行して画面を表示する
void CommandContext::ExecuteAndPresent(IDXGISwapChain4* swapChain) {
	// コマンドリストの内容を確定させる。すべてのコマンドを積んでからCloseすること
	HRESULT hr = commandList_->Close();
	assert(SUCCEEDED(hr));

	/// --- キックする ---
	// GPUにコマンドリストの実行を行わせる
	ID3D12CommandList* commandLists[] = {commandList_.Get()};
	commandQueue_->ExecuteCommandLists(1, commandLists);
	// GPUとOSに画面の交換を行うよう通知する
	hr = swapChain->Present(1, 0);
	assert(SUCCEEDED(hr));

	// GPUの実行完了を待つ
	WaitForGPU();

	// 次のフレーム用のコマンドリストを準備
	Reset();
}

/// --- GPUとの同期 ---
// GPUがコマンドを完了するまで待つ
void CommandContext::WaitForGPU() {
	// GPUにSignalを送る
	// Fenceの値を更新
	fenceValue_++;
	// GPU画がここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
	HRESULT hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
	assert(SUCCEEDED(hr));
	// Fenceの値が指定したSignal値にたどり着いているか確認する
	// GetCompletedValueの初期値はFence作成時に渡した初期値
	if (fence_->GetCompletedValue() < fenceValue_) {
		// 指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する。
		hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
		assert(SUCCEEDED(hr));
		// イベントを待つ
		WaitForSingleObject(fenceEvent_, INFINITE);
	}
}

/// --- 次のコマンドの準備 ---
// CommandAllocatorとCommandListをリセット
void CommandContext::Reset() {
	HRESULT hr = commandAllocator_->Reset();
	assert(SUCCEEDED(hr));
	hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
	assert(SUCCEEDED(hr));
}

/// --- 終了処理 ---
// コマンド関連のリソースを解放
void CommandContext::Finalize() {
	if (fenceEvent_ != nullptr) {
		CloseHandle(fenceEvent_);
		fenceEvent_ = nullptr;
	}
	// COMオブジェクトはComPtrのデストラクタが自動解放する
}

#pragma once

#include <Windows.h>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

///// ----- CommandContext ----- /////

class CommandContext {
public:
	CommandContext() = default;
	~CommandContext();

	CommandContext(const CommandContext&) = delete;
	CommandContext& operator=(const CommandContext&) = delete;

	/// --- 初期化 ---
	// コマンドの記録と実行に必要なリソースを作成
	void Initialize(ID3D12Device* device);

	/// --- コマンドの実行 ---
	// コマンドを実行してGPUの完了を待つ
	void ExecuteAndWait();

	// コマンドを実行して画面を表示する
	void ExecuteAndPresent(IDXGISwapChain4* swapChain);

	/// --- 取得 ---
	// CommandQueueを取得
	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }

	// CommandListを取得
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }

	/// --- 終了処理 ---
	// コマンド関連のリソースを解放
	void Finalize();

private:
	/// --- GPUとの同期 ---
	// GPUがコマンドを完了するまで待つ
	void WaitForGPU();

	/// --- 次のコマンドの準備 ---
	// CommandAllocatorとCommandListをリセット
	void Reset();

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	uint64_t fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;
};

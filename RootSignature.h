#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <ostream>

///// ----- RootSignature ----- /////

class RootSignature {
public:
	RootSignature() = default;
	~RootSignature();

	RootSignature(const RootSignature&) = delete;
	RootSignature& operator=(const RootSignature&) = delete;

	/// --- 初期化 ---
	// RootSignatureを作成
	void Initialize(ID3D12Device* device, std::ostream& logStream);

	/// --- 取得 ---
	// 作成したRootSignatureを取得
	ID3D12RootSignature* Get() const { return rootSignature_.Get(); }

	/// --- 終了処理 ---
	// RootSignatureで使用したリソースを解放
	void Finalize();

private:
	// RootSignature関連のCOMオブジェクトを自動解放する
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob_;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob_;
};

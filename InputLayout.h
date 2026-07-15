#pragma once

#include <d3d12.h>

///// ----- InputLayout ----- /////

class InputLayout {
public:
	/// --- 初期化 ---
	// 頂点データの並びを設定
	void Initialize();

	/// --- 取得 ---
	// InputLayoutの設定を取得
	const D3D12_INPUT_LAYOUT_DESC& GetDesc() const { return inputLayoutDesc_; }

private:
	D3D12_INPUT_ELEMENT_DESC inputElementDescs_[2]{};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc_{};
};

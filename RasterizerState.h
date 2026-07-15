#pragma once

#include <d3d12.h>

///// ----- RasterizerState ----- /////

class RasterizerState {
public:
	/// --- 初期化 ---
	// 面の判定と塗りつぶし方法を設定
	void Initialize();

	/// --- 取得 ---
	// RasterizerStateの設定を取得
	const D3D12_RASTERIZER_DESC& GetDesc() const { return rasterizerDesc_; }

private:
	D3D12_RASTERIZER_DESC rasterizerDesc_{};
};

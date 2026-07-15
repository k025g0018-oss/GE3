#pragma once

#include <d3d12.h>

///// ----- BlendState ----- /////

class BlendState {
public:
	/// --- 初期化 ---
	// 色の書き込み方法を設定
	void Initialize();

	/// --- 取得 ---
	// BlendStateの設定を取得
	const D3D12_BLEND_DESC& GetDesc() const { return blendDesc_; }

private:
	D3D12_BLEND_DESC blendDesc_{};
};

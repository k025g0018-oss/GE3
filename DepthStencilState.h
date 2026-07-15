#pragma once

#include <d3d12.h>

///// ----- DepthStencilState ----- /////

class DepthStencilState {
public:
	/// --- 初期化 ---
	// 奥行きの比較と書き込み方法を設定
	void Initialize();

	/// --- 取得 ---
	// DepthStencilStateの設定を取得
	const D3D12_DEPTH_STENCIL_DESC& GetDesc() const { return depthStencilDesc_; }

	// DSVで使用するFormatを取得
	DXGI_FORMAT GetFormat() const { return DXGI_FORMAT_D24_UNORM_S8_UINT; }

private:
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc_{};
};

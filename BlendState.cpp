#include "BlendState.h"

///// ----- BlendState ----- /////

/// --- 初期化 ---
// BlendStateの設定
void BlendState::Initialize() {
	// 全ての色要素を書き込む
	blendDesc_.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
}

#include "RasterizerState.h"

///// ----- RasterizerState ----- /////

/// --- 初期化 ---
// RasterizerStateの設定
void RasterizerState::Initialize() {
	// 裏面(時計回り)を表示しない
	rasterizerDesc_.CullMode = D3D12_CULL_MODE_BACK;
	// 三角形の中を塗りつぶす
	rasterizerDesc_.FillMode = D3D12_FILL_MODE_SOLID;
}

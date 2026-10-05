#include "DirectInput.h"

#include <cassert>
#include <cstring>

DirectInput::DirectInput() = default;
DirectInput::~DirectInput() {

}

/// <summary>
/// 初期化処理
/// </summary>
void DirectInput::Initialize(HINSTANCE hInstance, HWND hwnd) {
	// DirectInput本体をメンバ変数へ生成する
	HRESULT hr = DirectInput8Create(
		hInstance,
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		reinterpret_cast<void**>(directInput_.GetAddressOf()),
		nullptr
	);
	assert(SUCCEEDED(hr));

	// キーボードデバイスの生成
	// メンバのDirectInput本体からキーボードデバイスを生成する
	hr = directInput_->CreateDevice(
		GUID_SysKeyboard,
		keyboard_.GetAddressOf(),
		nullptr
	);
	assert(SUCCEEDED(hr));

	// 入力データ形式のセット
	hr = keyboard_->SetDataFormat(&c_dfDIKeyboard); // 標準形式
	assert(SUCCEEDED(hr));

	// 排他制御レベルのセット
	hr = keyboard_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));
}

/// <summary>
/// 更新処理
/// </summary>
// 毎フレームのキーボード状態を取得する
void DirectInput::Update() {
	// 現在の入力状態を1フレーム前の状態として保存する
	std::memcpy(previousKeys_, keys_, sizeof(keys_));

	// キーボード情報の取得開始
	HRESULT hr = keyboard_->Acquire();

	// ウィンドウが非アクティブだった場合などは入力状態をリセットする
	if (FAILED(hr)) {
		std::memset(keys_, 0, sizeof(keys_));
		return;
	}

	// 全キーの入力状態を取得する
	// BYTE key[256] = {};
	hr = keyboard_->GetDeviceState(sizeof(keys_), keys_);

	// 入力取得に失敗した場合は押されていない状態にする
	if (FAILED(hr)) {
		std::memset(keys_, 0, sizeof(keys_));
	}

	// キーが押されているときの処理例
	/*
	// 数字の0キーが押されていたら
	if(key[DIK_0]){
		OutputDebugStringA("Hit 0\n"); // 出力ウィンドウに「Hit 0」と表示
	}
	*/

	// トリガー処理
	/*
	bool キーを押した状態か(uint8_t キー番号);
	*/
}

/// <summary>
/// 解放処理
/// </summary>
void DirectInput::Finalize() {

	if (keyboard_) {
		keyboard_->Unacquire();
		keyboard_.Reset();
	}

	directInput_.Reset();
}

bool DirectInput::IsPress(uint8_t keyNumber) const {
	// 最上位ビットが立っていれば、現在キーが押されている
	return (keys_[keyNumber] & 0x80) != 0;
}

bool DirectInput::IsTrigger(uint8_t keyNumber) const {
	// 現在は押されていて、前フレームでは押されていなければトリガー
	const bool isCurrentPress = (keys_[keyNumber] & 0x80) != 0;
	const bool wasPreviousPress = (previousKeys_[keyNumber] & 0x80) != 0;

	return isCurrentPress && !wasPreviousPress;
}

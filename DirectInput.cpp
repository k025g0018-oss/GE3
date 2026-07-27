#include "DirectInput.h"

#include <cassert>

DirectInput::DirectInput() = default;
DirectInput::~DirectInput() {

}

/// <summary>
/// 初期化処理
/// </summary>
void DirectInput::Initialize(HINSTANCE hInstance, HWND hwnd) {
	/// --- DirectInputの初期化 ---

	IDirectInput8* directInput = nullptr;
	HRESULT hr = DirectInput8Create(hInstance, DIRECTINPUT_HEADER_VERSION, IID_IDirectInput8, reinterpret_cast<void**>(directInput_.GetAddressOf()), nullptr);
	assert(SUCCEEDED(hr));

	/// --- キーボードデバイスの生成 ---

	IDirectInputDevice8* keyboard = nullptr;
	hr = directInput->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), NULL);
	assert(SUCCEEDED(hr));

	/// --- 入力データ形式のセット ---

	hr = keyboard->SetDataFormat(&c_dfDIKeyboard); // 標準形式
	assert(SUCCEEDED(hr));

	/// --- 排他制御レベルのセット ---

	hr = keyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));
}
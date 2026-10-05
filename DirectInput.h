#pragma once

#include <Windows.h>
#include <wrl.h>
#include <cstdint>

#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

///// ----- DirectInput ----- /////

class DirectInput {
public:
	DirectInput();
	~DirectInput();

	/// <summary>
	/// 初期化処理
	/// </summary>
	// DirectInputの生成に必要なインスタンスとウィンドウを受け取る
	void Initialize(HINSTANCE hInstance, HWND hwnd);

	/// <summary>
	/// 更新処理
	/// </summary>
	// 毎フレームのキーボード状態を取得する
	void Update();

	/// <summary>
	/// 描画処理
	/// </summary>

	/// <summary>
	/// 解放処理
	/// </summary>
	// キーボード入力取得をやめて、デバイスとDirectInput本体を開放する
	void Finalize();

	/// <summary>
	/// キーが押されている間trueを返す
	/// </summary>
	bool IsPress(uint8_t keyNumber) const;

	/// <summary>
	/// キーを押した瞬間だけtrueを返す
	/// </summary>
	bool IsTrigger(uint8_t keyNumber) const;

private:
	// DirectInput本体を自動解放する
	Microsoft::WRL::ComPtr<IDirectInput8> directInput_;

	// 毎フレーム入力状態を取得するキーボードデバイス
	Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;

	// 現在のフレームのキー入力状態
	BYTE keys_[256]{};

	// 1フレーム前のキー入力状態
	BYTE previousKeys_[256]{};
};
#pragma once

#include <Windows.h>
#include <wrl.h>

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
	
	// キーが押されているか確認する
	bool IsPress(BYTE key) const;

private:
	// DirectInput本体を自動解放する
	Microsoft::WRL::ComPtr<IDirectInput8> directInput_;

	// 毎フレーム入力状態を取得するキーボードデバイス
	Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;

};
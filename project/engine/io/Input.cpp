#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include "Input.h"

#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")

std::unique_ptr <Input> Input::instance = nullptr;

// 共通の定数
const float STICK_MAX = 32768.0f;
const float DEAD_ZONE = 0.1f; // 10%以下の傾きは無視する

// 初期化
void Input::Initialize(WindowAPI* windowAPI) {
	// 借りてきたWindowAPIのインスタンスを記録
	this->windowAPI_ = windowAPI;

	HRESULT result;

	// DirectInputの初期化
	result = DirectInput8Create(
		windowAPI_->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8,
		(void**)&directInput, nullptr);
	assert(SUCCEEDED(result));

	// キーボードデバイスの生成
	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(result));

	// 入力データ形式のセット
	result = keyboard->SetDataFormat(&c_dfDIKeyboard); // 標準形式
	assert(SUCCEEDED(result));

	// 排他制御レベルのセット
	result = keyboard->SetCooperativeLevel(
		windowAPI_->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(result));
	
	// マウスデバイス生成
	result = directInput->CreateDevice(GUID_SysMouse, &mouse, nullptr);
	// データフォーマット設定
	result = mouse->SetDataFormat(&c_dfDIMouse);
	// 取得開始
	mouse->Acquire();

	// --- ゲームパッドの列挙 ---
// 接続されているジョイスティック（パッド）を探して、見つかるたびに EnumJoysticksCallback を呼ぶ
	result = directInput->EnumDevices(
		DI8DEVCLASS_GAMECTRL,
		// --- Initialize関数内のEnumDevicesの中 ---
		[](const DIDEVICEINSTANCE* pdidInstance, VOID* pContext) -> BOOL {
			auto self = static_cast<Input*>(pContext);
			ComPtr<IDirectInputDevice8> newPad;

			if (FAILED(self->directInput->CreateDevice(pdidInstance->guidInstance, &newPad, nullptr))) {
				return DIENUM_CONTINUE;
			}

			newPad->SetDataFormat(&c_dfDIJoystick);
			newPad->SetCooperativeLevel(self->windowAPI_->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);

			// ★追加：軸の範囲を設定する
			DIPROPRANGE diprg;
			diprg.diph.dwSize = sizeof(DIPROPRANGE);
			diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
			diprg.diph.dwHow = DIPH_DEVICE;
			diprg.diph.dwObj = 0;
			diprg.lMin = -32768; // 最小値
			diprg.lMax = 32768;  // 最大値
			newPad->SetProperty(DIPROP_RANGE, &diprg.diph);

			self->gamepads.push_back(newPad);
			self->padStates.emplace_back();

			return DIENUM_CONTINUE;
		},
		this, DIEDFL_ATTACHEDONLY);
}

// 更新
void Input::Update() {
	// 前回のキー入力を保存
	memcpy(keyPre, key, sizeof(key));

	// キーボード情報の取得開始
	keyboard->Acquire();
	// 全キーの入力情報を取得する
	keyboard->GetDeviceState(sizeof(key), key);

	// マウススクリーン座標取得
	POINT pos;
	GetCursorPos(&pos);
	ScreenToClient(windowAPI_->GetHwnd(), &pos);

	mouseScreenX = pos.x;
	mouseScreenY = pos.y;

	// マウスボタン
   // 現在の状態をクリアしてから取得
	ZeroMemory(&mouseState, sizeof(mouseState));

	HRESULT hr = mouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState);
	if (FAILED(hr)) {
		// フォーカスが外れた場合は再取得を試みる
		mouse->Acquire();
		mouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState);
	}

	// Update関数内のパッドループを以下に丸ごと入れ替え
	for (size_t i = 0; i < gamepads.size(); i++) {
		gamepads[i]->Poll();

		// 現在の状態を取得
		HRESULT hr = gamepads[i]->GetDeviceState(sizeof(DIJOYSTATE), &padStates[i]);

		// もし取得に失敗（ウィンドウからフォーカスが外れた等）したら再取得を試みる
		if (FAILED(hr)) {
			gamepads[i]->Acquire();
		}
	}
}

Input* Input::GetInstance() {
	if (instance == nullptr) {
		instance = std::make_unique <Input>();
	}
	return instance.get();
}

// プッシュ入力
bool Input::PushKey(BYTE keyNumBer) {
	// 指定キーを押していればtrueを返す
	if (key[keyNumBer]) {
		return true;
	}

	// 押していなければfalseを返す
	return false;
}
// トリガー入力
bool Input::TriggerKey(BYTE keyNumber) {
	// 指定キーが押されていて、前回は押されていなければtrueを返す
	if (key[keyNumber] && !keyPre[keyNumber]) {
		return true;
	}

	return false;
}

// マウスボタンが押されたかどうか
bool Input::IsMouseButtonPressed(int button) {
	if (button < 0 || button >= 4) return false;
	if (mouseState.rgbButtons[button] & 0x80) {
		return true;
	}
	return false;
}

// ゲームパッドのボタンが押されたかどうか
bool Input::IsPadButtonPressed(int padIndex, int button) {
	if (padIndex >= padStates.size()) return false;
	
	if (padStates[padIndex].rgbButtons[button]) {
		return true;
	}

	return false;
}

// ゲームパッドの軸の値を取得
float Input::GetPadLeftAxisX(int padIndex) {
	if (padIndex >= 0 && padIndex < static_cast<int>(padStates.size())) {
		float val = static_cast<float>(padStates[padIndex].lX) / STICK_MAX;
		return (abs(val) < DEAD_ZONE) ? 0.0f : val;
	}
	return 0.0f;
}
float Input::GetPadLeftAxisY(int padIndex) {
	if (padIndex >= 0 && padIndex < static_cast<int>(padStates.size())) {
		// Y軸は上がマイナスで返ってくることが多いため、ゲームに合わせて反転させることもある
		float val = static_cast<float>(padStates[padIndex].lY) / STICK_MAX;
		return (abs(val) < DEAD_ZONE) ? 0.0f : val;
	}
	return 0.0f;
}

float Input::GetPadRightAxisX(int padIndex) {
	if (padIndex >= 0 && padIndex < static_cast<int>(padStates.size())) {
		float val = static_cast<float>(padStates[padIndex].lRx) / STICK_MAX;
		return (abs(val) < DEAD_ZONE) ? 0.0f : val;
	}
	return 0.0f;
}

float Input::GetPadRightAxisY(int padIndex) {
	if (padIndex >= 0 && padIndex < static_cast<int>(padStates.size())) {
		float val = static_cast<float>(padStates[padIndex].lRy) / STICK_MAX;
		return (abs(val) < DEAD_ZONE) ? 0.0f : val;
	}
	return 0.0f;
}


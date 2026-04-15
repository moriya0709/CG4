#pragma once
#include <cstdint>
#include <string>
#include <cassert>
#include <random>

#include "Calc.h"

struct Emitter {
	Transform transform; //!< エミッタのTransform
	uint32_t count; //!< 発生数
	float frequency; //!< 発生頻度
	float frequencyTime; //!< 頻度用時刻
	std::string name; //!< エミッタ名
};

class ParticleEmitter {
public:
	// 初期化
	void Initialize(std::string name, const Transform& transform, uint32_t count, float frequency);
	// 更新
	void Update();

	// パーティクル発生
	void Emit();
	// アクティブ設定
	void SetActive(const std::string& name);

	// セーブ
	void SaveParticle(const std::string& filePath);
	// ロード
	void LoadParticle(const std::string& filePath);

	// エディター
	void Editor();

	// setter
	void SetTranslate(Vector3 translate) { emitter.transform.translate = translate; }

private:
	// パーティクルエミッタの設定
	Emitter emitter;
	// パーティクル共通データ
	std::uniform_real_distribution<float> distTranslate; // ランダムな座標範囲
	std::uniform_real_distribution<float> distVelocity; // ランダムな速度範囲
	std::uniform_real_distribution<float> distTime = std::uniform_real_distribution(1.0f, 3.0f); // ランダムな寿命範囲
	Vector3 ifTranslate; // ランダムな座標にするかどうか
	Vector3 velocity; // ランダムに動かすかどうか
	Vector4 color; // 色
	Vector4 finalColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	float colorChangeSpeed = 1.0f;
	float lifeTime; // 寿命
	bool isRandTranslate[3] = { true }; // ランダムな座標にするかどうか
	bool isRandVelocity[3] = { true }; // ランダムに動かすかどうか
	bool isColorChange[3] = { false }; // 色変更するかどうか
	bool isScaleChange[3] = { false }; // スケール変更するかどうか
	float scaleAdd = 0.005f; // スケール変更量

	char fileName[20]; //パーティクルのファイル名
	

	// デルタタイム(60fps固定)
	const float kDeltaTime = 1.0f / 60.0f;
	
};
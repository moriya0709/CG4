#include "ParticleEmitter.h"
#include "particleManager.h"
#include "ImGuiManager.h"
#include <iostream>

void ParticleEmitter::Initialize(std::string name, const Transform& transform, uint32_t count, float frequency) {
	emitter.transform = transform;
	emitter.count = count;
	emitter.frequency = frequency;
	emitter.frequencyTime = 0.0f;
	emitter.name = name;
}

void ParticleEmitter::Update() {
	ifTranslate = { 0.0f };
	velocity = { 0.0f };

	// ランダムにするかどうか
	if (isRandTranslate[0]) {
		ifTranslate.x = 1.0f;
	}
	if (isRandTranslate[1]) {
		ifTranslate.y = 1.0f;
	}
	if (isRandTranslate[2]) {
		ifTranslate.z = 1.0f;
	}

	// ランダムにするかどうか
	if (isRandVelocity[0]) {
		velocity.x = 1.0f;
	}
	if (isRandVelocity[1]) {
		velocity.y = 1.0f;
	}
	if (isRandVelocity[2]) {
		velocity.z = 1.0f;
	}

	// パーティクルの形状変化
	ParticleManager::GetInstance()->SetColorChange(isColorChange);
	ParticleManager::GetInstance()->SetFinalColor(finalColor);
	ParticleManager::GetInstance()->SetColorChangeSpeed(colorChangeSpeed); // 【追加】速度をManagerに送る
	ParticleManager::GetInstance()->SetScaleChange_(isScaleChange);

	// 時間経過によって発生させる
	emitter.frequencyTime += kDeltaTime; // 時刻を進める
	if (emitter.frequency <= emitter.frequencyTime) { // 頻度より大きいなら発生
		ParticleManager::GetInstance()->Emit(
			emitter.name,
			emitter.transform.translate,
			emitter.count,
			distTranslate,
			distVelocity,
			distTime,
			ifTranslate,
			velocity, color
		);

		emitter.frequencyTime -= emitter.frequency; // 余計に過ぎた時間も紙して頻度計算する
	}
}

void ParticleEmitter::Emit() {
	ParticleManager::GetInstance()->Emit(
		emitter.name,
		emitter.transform.translate,
		emitter.count,
		distTranslate,
		distVelocity, 
		distTime,
		ifTranslate, 
		velocity, color
	);

}

// アクティブ設定
void ParticleEmitter::SetActive(const std::string& name) {
	emitter.name = name;
}

void ParticleEmitter::SaveParticle(const std::string& filePath) {
	std::ofstream file(filePath, std::ios::binary);
	assert(file.is_open());

	// パーティクルの座標
	file << emitter.transform.translate.x << "," << emitter.transform.translate.y << "\n";
	// パーティクルの発生数
	file << emitter.count << "\n";
	// パーティクルの発生頻度
	file << emitter.frequency << "\n";
	// パーティクルのランダム座標
	file << isRandTranslate[0] << "," << isRandTranslate[1] << "," << isRandTranslate[2] << "\n";
	// パーティクルのランダム速度
	file << isRandVelocity[0] << "," << isRandVelocity[1] << "," << isRandVelocity[2] << "\n";
	// パーティクルの色
	file << color.x << "," << color.y << "," << color.z << "," << color.w << "\n";
	// パーティクルの最終色【追加】
	file << finalColor.x << "," << finalColor.y << "," << finalColor.z << "," << finalColor.w << "\n";
	// パーティクルの色変化速度
	file << colorChangeSpeed << "\n";
	// パーティクルの色変化
	file << isColorChange[0] << "," << isColorChange[1] << "," << isColorChange[2] << "\n";
	// パーティクルのサイズ変化
	file << isScaleChange[0] << "," << isScaleChange[1] << "," << isScaleChange[2] << "\n";
	// パーティクルの発生範囲
	file << distTranslate.a() << "," << distTranslate.b() << "\n";
	// パーティクルの速度範囲
	file << distVelocity.a() << "," << distVelocity.b() << "\n";
	// パーティクルのサイズ追加数
	file << scaleAdd << "\n";


	file.close();
}

void ParticleEmitter::LoadParticle(const std::string& filePath) {
	// ファイル読み込み
	std::ifstream file(filePath);
	assert(file.is_open());

	std::string line;

	// パーティクルの座標
	if (std::getline(file, line)) {
		auto s = line.find(',');
		emitter.transform.translate.x = std::stof(line.substr(0, s));
		emitter.transform.translate.y = std::stof(line.substr(s + 1));
	}

	// パーティクルの発生数
	if (std::getline(file, line)) {
		emitter.count = std::stoi(line);
	}

	// パーティクルの発生頻度
	if (std::getline(file, line)) {
		emitter.frequency = std::stof(line);
	}

	// ランダム座標（bool）
	if (std::getline(file, line)) {
		int a, b, c;
		sscanf_s(line.c_str(), "%d,%d,%d", &a, &b, &c);
		isRandTranslate[0] = (a != 0);
		isRandTranslate[1] = (b != 0);
		isRandTranslate[2] = (c != 0);
	}

	// ランダム速度（bool）
	if (std::getline(file, line)) {
		int a, b, c;
		sscanf_s(line.c_str(), "%d,%d,%d", &a, &b, &c);
		isRandVelocity[0] = (a != 0);
		isRandVelocity[1] = (b != 0);
		isRandVelocity[2] = (c != 0);
	}

	// 色
	if (std::getline(file, line)) {
		sscanf_s(line.c_str(), "%f,%f,%f,%f", &color.x, &color.y, &color.z, &color.w);
	}

	// 最終色
	if (std::getline(file, line)) {
		sscanf_s(line.c_str(), "%f,%f,%f,%f", &finalColor.x, &finalColor.y, &finalColor.z, &finalColor.w);
	}

	// 色変化速度【追加】
	if (std::getline(file, line)) {
		sscanf_s(line.c_str(), "%f", &colorChangeSpeed);
	}

	// 色変化（bool）
	if (std::getline(file, line)) {
		int a, b, c;
		sscanf_s(line.c_str(), "%d,%d,%d", &a, &b, &c);
		isColorChange[0] = (a != 0);
		isColorChange[1] = (b != 0);
		isColorChange[2] = (c != 0);
	}

	// サイズ変化（bool）
	if (std::getline(file, line)) {
		int a, b, c;
		sscanf_s(line.c_str(), "%d,%d,%d", &a, &b, &c);
		isScaleChange[0] = (a != 0);
		isScaleChange[1] = (b != 0);
		isScaleChange[2] = (c != 0);
	}

	// 発生範囲
	if (std::getline(file, line)) {
		float a, b;
		sscanf_s(line.c_str(), "%f,%f", &a, &b);
		distTranslate = std::uniform_real_distribution<float>(a, b);
	}

	// 速度範囲
	if (std::getline(file, line)) {
		float a, b;
		sscanf_s(line.c_str(), "%f,%f", &a, &b);
		distVelocity = std::uniform_real_distribution<float>(a, b);
	}

	// サイズ追加数
	if (std::getline(file, line)) {
		scaleAdd = std::stof(line);
	}

	file.close();
}

void ParticleEmitter::Editor() {
#ifdef USE_IMGUI
	ImGui::Begin("Partocle");
	// パーティクルの座標変更
	ImGui::DragFloat3("EmitterTranslate", &emitter.transform.translate.x, 0.01f, -100.0f, 100.0f);

	// パーティクルの状態
	if (ImGui::Button("FIRE", ImVec2(50, 50))) {
		LoadParticle("Resource/Particle/fire.csv");
	}
	ImGui::SameLine(); // 横並びにする
	if (ImGui::Button("Explosion", ImVec2(50, 50))) {
		LoadParticle("Resource/Particle/explosion.csv");
	}
	ImGui::SameLine(); // 横並びにする
	if (ImGui::Button("Snow", ImVec2(50, 50))) {
		LoadParticle("Resource/Particle/snow.csv");
	}

	// パーティクルの発生数
	ImGui::SliderInt("EmitterCount", (int*)&emitter.count, 1, 100);
	// パーティクルの発生頻度
	ImGui::SliderFloat("EmitterFrequency", &emitter.frequency, 0.01f, 5.0f);
	// パーティクルのランダム座標
	ImGui::Checkbox("randTranslate.x", &isRandTranslate[0]);
	ImGui::Checkbox("randTranslate.y", &isRandTranslate[1]);
	ImGui::Checkbox("randTranslate.z", &isRandTranslate[2]);
	// パーティクルのランダム速度
	ImGui::Checkbox("randVelocity.x", &isRandVelocity[0]);
	ImGui::Checkbox("randVelocity.y", &isRandVelocity[1]);
	ImGui::Checkbox("randVelocity.z", &isRandVelocity[2]);
	// パーティクルの色
	ImGui::ColorEdit4("ParticleColor", &color.x);
	ImGui::ColorEdit4("ParticleFinalColor", &finalColor.x); // 【追加】最終色を設定するGUI
	ImGui::SliderFloat("ColorChangeSpeed", &colorChangeSpeed, 1.0f, 20.0f);
	// パーティクルの色変化
	ImGui::Checkbox("colorChange.x", &isColorChange[0]);
	ImGui::Checkbox("colorChange.y", &isColorChange[1]);
	ImGui::Checkbox("colorChange.z", &isColorChange[2]);
	// パーティクルのサイズ変化

	ImGui::Checkbox("scaleChange.x", &isScaleChange[0]);
	ImGui::Checkbox("scaleChange.y", &isScaleChange[1]);
	ImGui::Checkbox("scaleChange.z", &isScaleChange[2]);


	// パーティクルの発生範囲
	ImGui::SliderFloat2("distTranslate", (float*)&distTranslate, -100.0f, 100.0f);
	// パーティクルの速度範囲
	ImGui::SliderFloat2("distVelocity", (float*)&distVelocity, -100.0f, 100.0f);
	// パーティクルのサイズ追加数
	ImGui::SliderFloat("scaleAdd", &scaleAdd, -0.05f, 0.05f);

	// ファイル名
	ImGui::InputText("FileName", fileName, IM_ARRAYSIZE(fileName));

	// セーブ
	if (ImGui::Button("SaveParticles")) {
		std::string path = "Resource/Particle/";
		path += fileName;
		path += ".csv";   // 拡張子を自動付与

		SaveParticle(path);
	}
	// ロード
	if (ImGui::Button("LoadParticles")) {
		std::string path = "Resource/Particle/";
		path += fileName;
		path += ".csv";   // 拡張子を自動付与

		LoadParticle(path);
	}
	ImGui::End();

#endif
}

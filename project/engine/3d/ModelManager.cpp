#include "ModelManager.h"
#include "ModelCommon.h"
#include "Model.h"
#include "DirectXCommon.h"

std::unique_ptr <ModelManager> ModelManager::instance = nullptr;

void ModelManager::Initialize(DirectXCommon* dxCommon) {
	dxCommon_ = dxCommon;

	modelCommon = new ModelCommon();
	modelCommon->Initialize(dxCommon);
}

// シングルトンインスタンスの取得
ModelManager* ModelManager::GetInstance() {
	if (instance == nullptr) {
		instance = std::make_unique <ModelManager>();
	}
	return instance.get();
}

// モデルファイルの読み込み
void ModelManager::LoadModel(const std::string& directoryPath, const std::string& filePath) {
	// 読み込み済みモデルを検索
	if (models.contains(filePath)) {
		// 読み込み済みなら早期return
		return;
	}

	// モデルの生成とファイル読み込み、初期化
	std::unique_ptr<Model>model = std::make_unique<Model>();
	model->Initialize(modelCommon, dxCommon_, directoryPath, filePath);

	// モデルをmapコンテナに格納する
	models.insert(std::make_pair(filePath, std::move(model)));


}

// モデルの検索
Model* ModelManager::FindModel(const std::string& filePath) {
	// 読み込み済みモデルを検索
	if (models.contains(filePath)) {
		// 読み込みモデルを戻り値としてreturn
		return models.at(filePath).get();
	}

	// ファイル名一致なし
	return nullptr;
}

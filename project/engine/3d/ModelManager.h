#pragma once
#include <map>
#include <string>
#include <memory>

class Model;
class ModelCommon;
class DirectXCommon;

class ModelManager {
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon);

	// シングルトンインスタンスの取得
	static ModelManager* GetInstance();

	// モデルファイルの読み込み
	void LoadModel(const std::string& directoryPath, const std::string& filePath);
	// モデルの検索
	Model* FindModel(const std::string& filePath);
	
	ModelManager() = default;
	~ModelManager() = default;
	ModelManager(ModelManager&) = delete;
	ModelManager& operator=(ModelManager&) = delete;

private:
	static std::unique_ptr <ModelManager> instance;
	// モデルデータ
	std::map<std::string, std::unique_ptr<Model>> models;


	// モデル共通部
	ModelCommon* modelCommon = nullptr;
	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;

};


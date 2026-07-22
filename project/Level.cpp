#include "Level.h"

void Level::LoadJson(const std::string fileName) {
	// jsonファイルのデシリアライズ化

	// 連結してフルパスを得る
	const std::string fullpath = "Resource/levels/" + fileName + ".json";

	// ファイルストリーム
	std::ifstream file;

	// ファイルを開く
	file.open(fullpath);
	// ファイルオープン失敗をチェック
	if (file.fail()) {
		assert(0);
	}
	
	nlohmann::json deserialized;

	// ファイルから読み込み、メモリへ格納
	file >> deserialized;

	// 正しいレベルデータファイルかチェック
	assert(deserialized.is_object());

	assert(deserialized.contains("name"));
	assert(deserialized["name"].is_string());

	// *レベルデータを構造体に格納していく* //
	levelData = new LevelData();

	// "name"を文字列として取得
	levelData->name = deserialized["name"].get<std::string>();
	assert(levelData->name == "scene");

	// "objects"の全オブジェクトを走査
	// "objects"の全オブジェクトを走査
	for (nlohmann::json& object : deserialized["objects"]) {
		// オブジェクト1つ分の妥協性のチェック
		assert(object.contains("type"));

		// 読み込んだ type を取得
		std::string objType = object["type"].get<std::string>();

		// 大文字・小文字のブレに対応
		if (objType == "MESH" || objType == "mesh") {

			// 1個分の要素を一時的に作成
			ObjectData newData{};
			newData.type = objType;
			newData.name = object["name"].get<std::string>();

			// トランスフォームのパラメータ読み込み
			nlohmann::json& transform = object["transform"];
			// 平行移動 "translation" (YとZを入れ替えている仕様に合わせています)
			newData.transform.translate.x = (float)transform["translation"][0];
			newData.transform.translate.y = (float)transform["translation"][2];
			newData.transform.translate.z = (float)transform["translation"][1];

			newData.transform.rotate.x = (float)transform["rotation"][0];
			newData.transform.rotate.y = (float)transform["rotation"][2];
			newData.transform.rotate.z = (float)transform["rotation"][1];

			newData.transform.scale.x = (float)transform["scaling"][0];
			newData.transform.scale.y = (float)transform["scaling"][2];
			newData.transform.scale.z = (float)transform["scaling"][1];

			// "file_name"
			if (object.contains("file_name")) {
				newData.file_name = object["file_name"].get<std::string>();
			}

			// 配列に追加
			levelData->objects.push_back(newData);
		}
	}

}

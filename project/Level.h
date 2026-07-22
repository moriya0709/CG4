#pragma once
#include <string>
#include <json.hpp>
#include <fstream>
#include <cassert>
#include <vector>

#include "Calc.h"

// オブジェクト 1個分のデータ
struct ObjectData {
	std::string type;
	std::string name;
	Transform transform;
	std::string file_name;
};

// レベルデータ
struct LevelData {
	// "name"
	std::string name;
	// "objects"
	std::vector<ObjectData> objects;
};

class Level {
public:
	// JSONファイル読み込み
	void LoadJson(const std::string fileName);

	// getter
	ObjectData GetObjectData() const { return objectData; }
	LevelData* GetLevelData() const { return levelData; }

private:
	ObjectData objectData;
	LevelData* levelData;

};


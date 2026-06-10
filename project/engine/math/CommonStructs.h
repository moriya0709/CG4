#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "Calc.h"

// 座標変換行列データ
struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Matrix4x4 prevWVP;
};
// テクスチャ
struct MaterialData {
	std::string textureFilePath;
	uint32_t textureIndex = 0;
	Vector3 emissive;
};
// 頂点データ
struct VertexData {
	Vector4 position; // 頂点座標
	Vector2 texcoord; // テクスチャ座標
	Vector3 normal; // 正規化座標
	Vector3 outlineNormal;   // 第二法線
};
// ノードデータ
struct Node {
    Matrix4x4 localMatrix; // ローカル変換行列
    std::string name; // ノードの名前
    std::vector<Node> children; // 子ノードのリスト
};
// モデルデータ
struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
	Node rootNode;
};
// マテリアルデータ
struct Material {
    Vector4 color;             // 16バイト (0-15)
    int32_t enableLighting;    // 4バイト (16-19)
    int32_t enableToonShading; // 4バイト (20-23)
    Vector2 pad1;              // 8バイト (24-31) - 16バイト境界に合わせる
    Matrix4x4 uvTransform;     // 64バイト (32-95)
    Vector3 emissive;          // 12バイト (96-107)
    float shininess;           // 4バイト (108-111)

    Vector4 fresnelColor;      // 16バイト (112-127)
    float fresnelPower;        // 4バイト (128-131)
    float pad2[3];             // 12バイト (132-143) - rimColorを16倍数へ押し出し

    Vector4 rimColor;          // 16バイト (144-159)
    float rimThreshold;        // 4バイト (160-163)
    float pad3[3];             // 12バイト (164-175) - 次の変数を16倍数へ押し出し

    // ★ enviromentTexture を削除

    float environmentCoefficient; // 4バイト (176-179)
    float pad4[3];                // 12バイト (180-191) - 構造体末尾を16の倍数に合わせる
};

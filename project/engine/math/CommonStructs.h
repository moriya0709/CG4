#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <optional>

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
struct EulerTransform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};
struct QuaternionTransform {
    Vector3 scale;
    Quaternion rotate;
    Vector3 translate;
};
// ノードデータ
struct Node {
    QuaternionTransform transform;
    Matrix4x4 localMatrix; // ローカル変換行列
    std::string name; // ノードの名前
    std::vector<Node> children; // 子ノードのリスト
};
// モデルデータ
struct ModelData {
	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
	MaterialData material;
	Node rootNode;
};
// マテリアルデータ
struct Material {
    Vector4 color;             // 16バイト (0-15)
    int32_t enableLighting;    // 4バイト (16-19)
    int32_t enableToonShading; // 4バイト (20-23)
    int32_t useNoise;
    float pad1;              // 8バイト (24-31) - 16バイト境界に合わせる
    Matrix4x4 uvTransform;     // 64バイト (32-95)
    Vector3 emissive;          // 12バイト (96-107)
    float shininess;           // 4バイト (108-111)

    Vector4 fresnelColor;      // 16バイト (112-127)
    float fresnelPower;        // 4バイト (128-131)
    Vector3 pad2;             // 12バイト (132-143) - rimColorを16倍数へ押し出し

    Vector4 rimColor;          // 16バイト (144-159)
    float rimThreshold;        // 4バイト (160-163)
    Vector3 pad3;             // 12バイト (164-175) - 次の変数を16倍数へ押し出し

    // ★ enviromentTexture を削除

    float environmentCoefficient; // 4バイト (176-179)
    Vector3 pad4;                // 12バイト (180-191) - 構造体末尾を16の倍数に合わせる

    Vector3 burnColor;
    float pad5;

};
// カメラデータ
struct ViewData {
    Vector3 cameraPos;
    float pad;
};
struct Joint {
    QuaternionTransform transform;  // Transform情報
    Matrix4x4 localMatrix;
    Matrix4x4 skeletonSpaceMatrix;  // skeletonSpaceでの変換行列
    std::string name;               // 名前
    std::vector<int32_t> children;  // 子JointのIndexのリスト
    int32_t index;                  // 自身のIndex
    std::optional<int32_t> parent;  // 親JointのIndex
};
struct Skeleton {
    int32_t root;                               // RootJointのINdex
    std::map<std::string, int32_t> jointMap;    // Joint名とIndexとの辞書
    std::vector<Joint> joints;                 // 所属しているジョイント
};

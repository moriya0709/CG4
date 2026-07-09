#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <D3d12.h>
#include <cassert>
#include <memory>
#include <array>
#include <wrl.h>
#include <dxcapi.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Calc.h"
#include "CommonStructs.h"
#include "AnimationManager.h"

class ModelCommon;
class DirectXCommon;
class SrvManager;
class AnimationManager;

class Model {
public:
	// 初期化
	void Initialize(ModelCommon* modelCommon, DirectXCommon* dxCommon, SrvManager* srvManager,const std::string& directoryPath,const std::string& filename);
	// 更新
	void Update();
	// 描画
	void Draw();

	// .mtlファイルの読み込み
	MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	// モデルファイルの読み込み
	ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);

	// 第二法線生成
	void GenerateOutlineNormal(std::vector<VertexData>& vertices);

	// ノードの読み込み
	Node ReadNode(aiNode* node);
	// Skeleton生成
	Skeleton CreateSkeleton(const Node& rootNode);
	// Joint生成
	int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);
	// SKinCluster生成
	SkinCluster CreateSkinCluster(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const Skeleton& skeleton, const ModelData& modelData, const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize);

	// アニメーションを適用する
	void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime);

	// getter
	ModelData GetModelData() const { return modelData; }
	bool IsSkinning() const {
		return !modelData.skinClusterData.empty();
	}
	SkinCluster GetSkinCluster() const { return skinCluster; }

private:
	// Objファイルのデータ
	ModelData modelData;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;

	// バッファリソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;
	Material* materialData = nullptr;

	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;

	// 環境マップ用テクスチャのファイルパス
	std::string enviromentTexture;

	// index
	uint32_t srvIndex_;

	// スキンクラスタ
	SkinCluster skinCluster;

	// エミッシブが有効か
	bool isEmissive = false;

	// ModelCommonのポインタ
	ModelCommon* modelCommon_ = nullptr;
	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;
	// SrvManagerのポインタ
	SrvManager* srvManager_ = nullptr;
	// AnimationManagerのポインタ
	std::unique_ptr <AnimationManager> animationManager_ = nullptr;
	Animation animation;

	// スケルトン
	Skeleton skeleton;
};


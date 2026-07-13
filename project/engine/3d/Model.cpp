#include "Model.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "SkyBox.h"
#include "AnimationManager.h"
#include "LineCommon.h"
#include "CameraManager.h"
#include "Camera.h"
#include "SrvManager.h"
#include "ObjectCommon.h"
#include <assimp/vector3.h>
#include <assimp/matrix4x4.h>
#include <assimp/quaternion.h>
#include <cassert>
#include <algorithm>

void Model::Initialize(ModelCommon* modelCommon, DirectXCommon* dxCommon, SrvManager* srvManager, const std::string& directoryPath, const std::string& filename) {
	// 引数で受け取ってメンバ変数に記録する
	modelCommon_ = modelCommon;
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	animationManager_ = std::make_unique <AnimationManager>();

	// モデル読み込み
	modelData = LoadModelFile(directoryPath, filename);
	// アニメーション読み込み
	animation = animationManager_->LoadAnimationFile(directoryPath, filename);
	// スケルトン生成
	skeleton = CreateSkeleton(modelData.rootNode);

	// アウトライン用法線生成
	GenerateOutlineNormal(modelData.vertices);

	// *頂点データ* //

	// ⚠️ Initialize関数内の「*頂点データ*」部分を以下のように書き換えます
// 【入力用】変形前の頂点リソース（SRV用構造化バッファ）
	inputVertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());
	VertexData* mappedInput = nullptr;
	inputVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedInput));
	std::memcpy(mappedInput, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

	// ⭕ 修正後: DirectXのAPIを直接叩いてUAV用のフラグ付きで作成する
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT; // UAVはGPU側で読み書きするため DEFAULT ヒープを使用

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeof(VertexData) * modelData.vertices.size(); // バッファ全体のサイズ
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	// ★最重要: コンピュートシェーダーからの書き込み（UAV）を許可するフラグ
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON, // または D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		nullptr,
		IID_PPV_ARGS(&outputVertexResource)
	);
	assert(SUCCEEDED(hr));
	// 描画で使う頂点バッファビュー(vertexBufferView)のターゲットを出力用(outputVertexResource)にしておく
	vertexBufferView.BufferLocation = outputVertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 【定数バッファ】4番用のSkinningInfoの作成
	skinningInfoResource = dxCommon_->CreateBufferResource(sizeof(SkinningInfo));
	skinningInfoResource->Map(0, nullptr, reinterpret_cast<void**>(&skinningInfoData));
	skinningInfoData->vertexCount = static_cast<uint32_t>(modelData.vertices.size());

	// *マテリアル* //

	// リソース
	materialResource = dxCommon_->CreateBufferResource(sizeof(Material));
	// 書き込む為のアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 初期値を書き込む
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = true;
	materialData->enableToonShading = true;
	materialData->uvTransform = MakeIdentity4x4();

	if (isEmissive) {
		materialData->emissive = modelData.material.emissive;
	} else {
		materialData->emissive = {0.0f,0.0f,0.0f};
	}

	// ハイライト
	materialData->shininess = 70.0f;

	materialData->fresnelColor = { 1.0f, 1.0f, 1.0f, 0.5f };
	materialData->fresnelPower = 4.0f;
	materialData->rimColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData->rimThreshold = 0.5f;
	// 環境マップ用テクスチャ
	enviromentTexture = "Resource/rostock_laage_airport_4k.dds";
	TextureManager::GetInstance()->LoadTexture(enviromentTexture);
	materialData->environmentCoefficient = 0.0f;
	
	// *インデックス* //
	indexResource = dxCommon_->CreateBufferResource(sizeof(uint32_t) * modelData.indices.size());
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = UINT(sizeof(uint32_t) * modelData.indices.size());
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	
	uint32_t* mappedIndex = nullptr;
	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndex));
	std::memcpy(mappedIndex, modelData.indices.data(), sizeof(uint32_t) * modelData.indices.size());

	// スキンクラスター
	skinCluster = CreateSkinCluster(dxCommon_->GetDevice(), skeleton, modelData, dxCommon_->GetSrvHeap(), dxCommon_->GetSrvDescriptorSize());
	// UAV生成
	CreateUav();


	// *テクスチャ* //

	// 読み込み
	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);
	// 番号取得
	modelData.material.textureIndex = TextureManager::GetInstance()->GetSrvIndex(modelData.material.textureFilePath);

}

void Model::Update() {
	// アニメーションが無い、または再生時間が0の時は何もしない
	if (animation.duration > 0.0f && !animation.nodeAnimations.empty()) {
		// アニメーション更新
		animationManager_->animationTime = (std::max)(0.0f, animationManager_->animationTime + 1.0f / 60.0f);
		if(animationManager_->animationTime >= animation.duration) {
			animationManager_->animationTime = 0.0f;
		}

		ApplyAnimation(skeleton, animation, animationManager_->animationTime);
	}

	// 全てのJointを更新。親が若いので通常ループで処理可能になっている
	for (Joint& joint : skeleton.joints) {
		joint.localMatrix = MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
		if (joint.parent) {
			joint.skeletonSpaceMatrix = joint.localMatrix * skeleton.joints[*joint.parent].skeletonSpaceMatrix;
		} else {
			joint.skeletonSpaceMatrix = joint.localMatrix;
		}
	}

	// スキンクラスタの更新
	for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
		assert(jointIndex < skinCluster.inverseBindPoseMatrices.size());
		skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix =
			skinCluster.inverseBindPoseMatrices[jointIndex] * skeleton.joints[jointIndex].skeletonSpaceMatrix;
		skinCluster.mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix =
			Transpose(Inverse(skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix));
	}

	DispatchSkinning();
}

void Model::Draw() {

	// RootSignatureを設定。PSOに設定しているけど別途設定が必要
	if (IsSkinning()) {
		D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {
			vertexBufferView,
			skinCluster.influenceBufferView
		};
		// アニメーション用：スロット0と1の「2つ」をセット
		dxCommon_->GetCommandList()->IASetVertexBuffers(0, 2, vbvs);
	} else {
		// 通常モデル用：スロット0の「1つ」だけをセット
		dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
	}
	// インデックスバッファビューを設定
	dxCommon_->GetCommandList()->IASetIndexBuffer(&indexBufferView);

	// マテリアルCBufferの場所を設定
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
	// SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である。
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(modelData.material.textureFilePath));
	// 環境マップ用テクスチャのセット
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(10, TextureManager::GetInstance()->GetSrvHandleGPU(enviromentTexture));

	// 描画
	dxCommon_->GetCommandList()->DrawIndexedInstanced(static_cast<UINT>(modelData.indices.size()), 1, 0, 0, 0);

}

// .mtlファイルの読み込み
MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	MaterialData materialData; // 構築するMaterialData
	std::string line; // ファイルから読んだ１行を格納するもの
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず聞けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// 拡散テクスチャ
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			materialData.textureFilePath =
				directoryPath + "/" + textureFilename;
		}
		// エミッシブカラー
		else if (identifier == "Ke") {
			s >> materialData.emissive.x
				>> materialData.emissive.y
				>> materialData.emissive.z;
		}


	}

	return materialData;
}

// モデルファイルの読み込み
ModelData Model::LoadModelFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData; // 構築するModelData
	std::vector<Vector4> positions; //位置
	std::vector<Vector3> normals; // 法線
	std::vector<Vector2> texcoords; //　テクスチャ座標
	std::string line; // ファイルから読んだ1行を格納するもの

	// ファイルを開く
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;

	// 拡張子を取得
	std::string ext = "";
	size_t dotIdx = filename.find_last_of('.');
	if (dotIdx != std::string::npos) {
		ext = filename.substr(dotIdx + 1);
		// 小文字化
		for (char& c : ext) {
			c = std::tolower(static_cast<unsigned char>(c));
		}
	}

	// 各フォーマットの判定フラグ
	bool isOBJ = (ext == "obj");
	bool isGLTF = (ext == "gltf" || ext == "glb");

	// Assimp読み込みフラグの決定
	unsigned int pFlags = aiProcess_FlipWindingOrder;
	if (isOBJ) {
		pFlags |= aiProcess_FlipUVs; // OBJの時だけUVを上下反転
	}

	// ファイルを読み込む
	const aiScene* scene = importer.ReadFile(filePath.c_str(), pFlags);
	assert(scene != nullptr && "ファイルの読み込みに失敗しました。");
	assert(scene->HasMeshes());

	// メッシュを解析する
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		aiMesh* mesh = scene->mMeshes[meshIndex];
		assert(mesh->HasNormals());
		assert(mesh->HasTextureCoords(0));

		const uint32_t baseVertex = static_cast<uint32_t>(modelData.vertices.size());
		modelData.vertices.resize(baseVertex + mesh->mNumVertices);   // 追記する

		for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
			aiVector3D& position = mesh->mVertices[vertexIndex];
			aiVector3D& normal = mesh->mNormals[vertexIndex];
			aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
			auto& v = modelData.vertices[baseVertex + vertexIndex];
			v.position = { -position.x, position.y, position.z, 1.0f };
			v.normal = { -normal.x, normal.y, normal.z };
			v.texcoord = { texcoord.x, texcoord.y };
		}

		// インデックスを解析
		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == 3);
			for (uint32_t element = 0; element < face.mNumIndices; ++element) {
				modelData.indices.push_back(baseVertex + face.mIndices[element]); // オフセットを足す
			}
		}

		// スキンクラスタを解析
		for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
			aiBone* bone = mesh->mBones[boneIndex];
			std::string jointName = bone->mName.C_Str();
			JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

			aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();
			aiVector3D scale, translate;
			aiQuaternion rotare;
			bindPoseMatrixAssimp.Decompose(scale, rotare, translate);
			Matrix4x4 bindPoseMatrix = MakeAffineMatrix(
				{ scale.x, scale.y, scale.z },
				{ rotare.x, -rotare.y, -rotare.z, rotare.w },
				{ -translate.x, translate.y, translate.z });
			jointWeightData.inverseBindPoseMatrix = Inverse(bindPoseMatrix);

			// weight情報を取り出す
			for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
				jointWeightData.vertexWeights.push_back({
					bone->mWeights[weightIndex].mWeight,
					baseVertex + bone->mWeights[weightIndex].mVertexId   // ここもオフセット
					});
			}
		}
	}

	// マテリアルを解析する
	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
		aiMaterial* material = scene->mMaterials[materialIndex];
		// ディフューズテクスチャの取得
		if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
			aiString textureFilePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
			modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
		}

		// エミッシブの取得
		aiColor3D emissiveColor(0.0f, 0.0f, 0.0f);
		if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS) {
			if (emissiveColor.r > 0.0f || emissiveColor.g > 0.0f || emissiveColor.b > 0.0f) {
				modelData.material.emissive.x = emissiveColor.r;
				modelData.material.emissive.y = emissiveColor.g;
				modelData.material.emissive.z = emissiveColor.b;

				// エミッシブを有効
				isEmissive = true;
			}
		}
	}

	// ノードを解析する
	modelData.rootNode = ReadNode(scene->mRootNode);
	

	return modelData;
}

void Model::GenerateOutlineNormal(std::vector<VertexData>& vertices) {
	const float epsilon = 0.0001f;

	for (size_t i = 0; i < vertices.size(); ++i) {

		Vector3 sumNormal = { 0,0,0 };

		for (size_t j = 0; j < vertices.size(); ++j) {

			// 座標がほぼ同じなら同一頂点とみなす
			if (fabs(vertices[i].position.x - vertices[j].position.x) < epsilon &&
				fabs(vertices[i].position.y - vertices[j].position.y) < epsilon &&
				fabs(vertices[i].position.z - vertices[j].position.z) < epsilon) {
				sumNormal += vertices[j].normal;
			}
		}

		vertices[i].outlineNormal = Normalize(sumNormal);
	}
}

Node Model::ReadNode(aiNode* node) {
	Node result;
	aiVector3D scale, translate;
	aiQuaternion rotate;
	node->mTransformation.Decompose(scale, rotate, translate);
	result.transform.scale = { scale.x,scale.y,scale.z };
	result.transform.rotate = { rotate.x,-rotate.y,-rotate.z, rotate.w }; // x軸を反転、さらに回転方向が逆なので軸を反転させる
	result.transform.translate = { -translate.x,translate.y,translate.z }; // x軸を反転
	result.localMatrix = MakeAffineMatrix(result.transform.scale, result.transform.rotate, result.transform.translate);

	result.name = node->mName.C_Str(); // Node名を格納
	result.children.resize(node->mNumChildren); // 子供の数だけ確保
	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
		// 再帰的に呼んで階層構造を作っていく
		result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
	}
	return result;
}

Skeleton Model::CreateSkeleton(const Node& rootNode) {
	Skeleton skeleton;
	skeleton.root = CreateJoint(rootNode, {}, skeleton.joints);

	// 名前とindexのマッピングを行いアクセスしやすくする
	for (const Joint& joint : skeleton.joints) {
		skeleton.jointMap.emplace(joint.name, joint.index);
	}

	return skeleton;
}

int32_t Model::CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints) {
	Joint joint;
	joint.name = node.name;
	joint.localMatrix = node.localMatrix;
	joint.skeletonSpaceMatrix = MakeIdentity4x4();
	joint.transform = node.transform;
	joint.index = int32_t(joints.size()); // 現在登録されている数をINdexに
	joint.parent = parent;
	joints.push_back(joint); // SkeletonのJoint列に追加
	for (const Node& child : node.children) {
		// 子Jointを生成し、そのIndexを登録
		int32_t childIndex = CreateJoint(child, joint.index, joints);
		joints[joint.index].children.push_back(childIndex);
	}
	// 自身のIndexを返す
	return joint.index;
}

SkinCluster Model::CreateSkinCluster(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const Skeleton& skeleton, const ModelData& modelData, const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize) {
	SkinCluster skinCluster;

	paletteSrvIndex_ = srvManager_->Allocate(1);
	inputVertexSrvIndex_ = srvManager_->Allocate(1);
	influenceSrvIndex_ = srvManager_->Allocate(1);
	outputVertexUavIndex_ = srvManager_->Allocate(1);

	// palette用のResourceを確保
	skinCluster.paletteResource = dxCommon_->CreateBufferResource(sizeof(WellForGPU) * skeleton.joints.size());
	WellForGPU* mappedPalette = nullptr;
	skinCluster.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
	skinCluster.mappedPalette = { mappedPalette,skeleton.joints.size() }; // spanを使ってアクセスするようにする
	skinCluster.paletteSrvHandle.first = dxCommon_->GetCPUDescriptorHandle(descriptorHeap, descriptorSize, paletteSrvIndex_);
	skinCluster.paletteSrvHandle.second = dxCommon_->GetGPUDescriptorHandle(descriptorHeap, descriptorSize, paletteSrvIndex_);

	// palette用のsrvを生成
	D3D12_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc{};
	paletteSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	paletteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	paletteSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	paletteSrvDesc.Buffer.FirstElement = 0;
	paletteSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	paletteSrvDesc.Buffer.NumElements = UINT(skeleton.joints.size());
	paletteSrvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);
	dxCommon_->GetDevice()->CreateShaderResourceView(skinCluster.paletteResource.Get(), &paletteSrvDesc, skinCluster.paletteSrvHandle.first);

	// inputVertex用のSRVを生成
	D3D12_SHADER_RESOURCE_VIEW_DESC inputSrvDesc{};
	inputSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	inputSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	inputSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	inputSrvDesc.Buffer.NumElements = UINT(modelData.vertices.size());
	inputSrvDesc.Buffer.StructureByteStride = sizeof(VertexData);
	dxCommon_->GetDevice()->CreateShaderResourceView(inputVertexResource.Get(), &inputSrvDesc,
		dxCommon_->GetCPUDescriptorHandle(descriptorHeap, descriptorSize, inputVertexSrvIndex_));

	// influence用のSRVを生成
	D3D12_SHADER_RESOURCE_VIEW_DESC influenceSrvDesc{};
	influenceSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	influenceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	influenceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	influenceSrvDesc.Buffer.NumElements = UINT(modelData.vertices.size());
	influenceSrvDesc.Buffer.StructureByteStride = sizeof(VertexInfluence);
	dxCommon_->GetDevice()->CreateShaderResourceView(skinCluster.influeceResouce.Get(), &influenceSrvDesc,
		dxCommon_->GetCPUDescriptorHandle(descriptorHeap, descriptorSize, influenceSrvIndex_));

	// influence用のResourceを確保
	skinCluster.influeceResouce = dxCommon_->CreateBufferResource(sizeof(VertexInfluence) * modelData.vertices.size());
	VertexInfluence* mappedInfluence = nullptr;
	skinCluster.influeceResouce->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
	std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * modelData.vertices.size()); // 0埋め。weightを0にしておく。
	skinCluster.mappedInfluence = { mappedInfluence,modelData.vertices.size() };

	// influence用のVBVを生成
	skinCluster.influenceBufferView.BufferLocation = skinCluster.influeceResouce->GetGPUVirtualAddress();
	skinCluster.influenceBufferView.SizeInBytes = UINT(sizeof(VertexInfluence) * modelData.vertices.size());
	skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);

	// InverseBindPoseMatrixの保存領域を作成
	skinCluster.inverseBindPoseMatrices.resize(skeleton.joints.size());
	std::generate(skinCluster.inverseBindPoseMatrices.begin(), skinCluster.inverseBindPoseMatrices.end(),MakeIdentity4x4);

	// ModelDataのSkinCluster情報を解析してinfluenceの中身を埋める
	for (const auto& jointWeight : modelData.skinClusterData) {
		auto it = skeleton.jointMap.find(jointWeight.first);
		if (it == skeleton.jointMap.end()) {
			continue;
		}

		skinCluster.inverseBindPoseMatrices[(*it).second] = jointWeight.second.inverseBindPoseMatrix;
		for (const auto& vertexWeight : jointWeight.second.vertexWeights) {
			auto& currentInfluence = skinCluster.mappedInfluence[vertexWeight.vertexIndex];
			for (uint32_t index = 0; index < kNumMaxInfluence; ++index) {
				if (currentInfluence.weights[index] == 0.0f) {
					currentInfluence.weights[index] = vertexWeight.weight;
					currentInfluence.jointIndices[index] = (*it).second;
					break;
				}
			}
		}


	}

	for (size_t i = 0; i < modelData.vertices.size(); ++i) {
		auto& currentInfluence = skinCluster.mappedInfluence[i];

		float weightSum = 0.0f;
		for (uint32_t j = 0; j < kNumMaxInfluence; ++j) {
			weightSum += currentInfluence.weights[j];
		}

		if (weightSum == 0.0f) {
			// どこにも影響されていない頂点は強制的に0番ボーンに追従させる
			currentInfluence.weights[0] = 1.0f;
			currentInfluence.jointIndices[0] = 0;
		} else {
			// 合計が1.0になるように正規化
			for (uint32_t j = 0; j < kNumMaxInfluence; ++j) {
				currentInfluence.weights[j] /= weightSum;
			}
		}
	}

	return skinCluster;

}

void Model::ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime) {
	for (Joint& joint : skeleton.joints) {
		// 対象のJointのAnimationがあれば、値の適用を行う。
		if (auto it = animation.nodeAnimations.find(joint.name); it != animation.nodeAnimations.end()) {
			const NodeAnimation& rootNodeAnimation = (*it).second;
			joint.transform.translate = animationManager_->CalculateValue(rootNodeAnimation.translate.keyframes, animationTime);
			joint.transform.rotate = animationManager_->CalculateValue(rootNodeAnimation.rotate.keyframes, animationTime);
			joint.transform.scale = animationManager_->CalculateValue(rootNodeAnimation.scale.keyframes, animationTime);
		}
	}
}

void Model::CreateUav() {
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = static_cast<UINT>(modelData.vertices.size());
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	uavDesc.Buffer.StructureByteStride = sizeof(VertexData);

	// 第二引数は今はnullptrにしておく
	dxCommon_->GetDevice()->CreateUnorderedAccessView(
		outputVertexResource.Get(), nullptr, &uavDesc,
		dxCommon_->GetCPUDescriptorHandle(dxCommon_->GetSrvHeap(), dxCommon_->GetSrvDescriptorSize(), outputVertexUavIndex_)
	);

}

void Model::DispatchSkinning() {
	// スキニングの必要がなければスキップ
	if (!IsSkinning()) return;

	auto commandList = dxCommon_->GetCommandList();
	auto srvHeap = dxCommon_->GetSrvHeap();

	// ディスクリプタヒープをセットする
	ID3D12DescriptorHeap* descriptorHeaps[] = { srvHeap };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	// パイプラインとルートシグネチャを設定
	auto objectCommon = ObjectCommon::GetInstance();
	commandList->SetComputeRootSignature(objectCommon->GetComputeRootSignature());
	commandList->SetPipelineState(objectCommon->GetComputePipelineState());

	// palette (SRV)
	commandList->SetComputeRootDescriptorTable(0, skinCluster.paletteSrvHandle.second);
	// inputVertex (SRV)
	commandList->SetComputeRootDescriptorTable(1, dxCommon_->GetGPUDescriptorHandle(srvHeap, dxCommon_->GetSrvDescriptorSize(), inputVertexSrvIndex_));
	// influence (SRV)
	commandList->SetComputeRootDescriptorTable(2, dxCommon_->GetGPUDescriptorHandle(srvHeap, dxCommon_->GetSrvDescriptorSize(), influenceSrvIndex_));
	// outputVertex (UAV)
	commandList->SetComputeRootDescriptorTable(3, dxCommon_->GetGPUDescriptorHandle(srvHeap, dxCommon_->GetSrvDescriptorSize(), outputVertexUavIndex_));
	// skinningInformation (CBV)
	commandList->SetComputeRootConstantBufferView(4, skinningInfoResource->GetGPUVirtualAddress());

	// 計算実行
	uint32_t numGroup = (static_cast<uint32_t>(modelData.vertices.size()) + 1023) / 1024;
	commandList->Dispatch(numGroup, 1, 1);

	// 計算終了のバリア
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.UAV.pResource = outputVertexResource.Get();
	commandList->ResourceBarrier(1, &barrier);
}

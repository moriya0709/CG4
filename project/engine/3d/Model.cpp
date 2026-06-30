#include "Model.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "SkyBox.h"
#include "AnimationManager.h"
#include "LineCommon.h"
#include "CameraManager.h"
#include "Camera.h"

void Model::Initialize(ModelCommon* modelCommon, DirectXCommon* dxCommon, const std::string& directoryPath, const std::string& filename) {
	// 引数で受け取ってメンバ変数に記録する
	modelCommon_ = modelCommon;
	dxCommon_ = dxCommon;
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

	// リソース
	vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());
	// バッファリソース
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	vertexBufferView.StrideInBytes = sizeof(VertexData);
	// 書き込む
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

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
	// ★ mtlから読んだ自己発光カラーを代入！
	materialData->emissive = modelData.material.emissive;

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

	// *テクスチャ* //

	// 読み込み
	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);
	// 番号取得
	modelData.material.textureIndex = TextureManager::GetInstance()->GetSrvIndex(modelData.material.textureFilePath);

}

void Model::Update() {
	// アニメーションが無い、または再生時間が0の時は何もしない
	if (animation.duration <= 0.0f || animation.nodeAnimations.empty()) {
		return;
	}

	// アニメーション更新
	animationManager_->animationTime += 1.0f / 60.0f;
	ApplyAnimation(skeleton, animation, animationManager_->animationTime);

	// 全てのJointを更新。親が若いので通常ループで処理可能になっている
	for (Joint& joint : skeleton.joints) {
		joint.localMatrix = MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
		if (joint.parent) {
			joint.skeletonSpaceMatrix = joint.localMatrix * skeleton.joints[*joint.parent].skeletonSpaceMatrix;
		} else {
			joint.skeletonSpaceMatrix = joint.localMatrix;
		}
	}
}

void Model::Draw() {

	// RootSignatureを設定。PSOに設定しているけど別途設定が必要
	dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView); // VBVを設定

	// マテリアルCBufferの場所を設定
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
	// SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である。
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(modelData.material.textureFilePath));
	// 環境マップ用テクスチャのセット
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(10, TextureManager::GetInstance()->GetSrvHandleGPU(enviromentTexture));

	// 描画
	dxCommon_->GetCommandList()->DrawInstanced(static_cast<UINT>(modelData.vertices.size()), 1, 0, 0);

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
		assert(mesh->HasNormals()); // 法線がないMeshは非対応
		assert(mesh->HasTextureCoords(0)); // TexcoordがないMeshは非対応

		// faceを解析する
		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
			aiFace& face = mesh->mFaces[faceIndex];

			// Vertexを解析する
			for(uint32_t element = 0; element < face.mNumIndices; ++element){
				uint32_t vertexIndex = face.mIndices[element];
				aiVector3D& position = mesh->mVertices[vertexIndex];
				aiVector3D& normal = mesh->mNormals[vertexIndex];
				aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
				VertexData vertex;
				vertex.position = { position.x, position.y, position.z, 1.0f };
				vertex.normal = { normal.x, normal.y, normal.z };
				vertex.texcoord = { texcoord.x,texcoord.y };
				// aiProcess_MakeLeftHandedはz*=-1で、右手->左手に変換するので手動で対処
				vertex.position.z *= -1.0f;
				vertex.normal.z *= -1.0f;

				if (isGLTF) {
					// glTFの左右反転を直すためにU座標を反転
					vertex.texcoord.x = 1.0f - vertex.texcoord.x;
				}

				modelData.vertices.push_back(vertex);

			}

		}


	}

	// マテリアルを解析する
	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
		aiMaterial* material = scene->mMaterials[materialIndex];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
			aiString textureFilePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
			modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
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
	result.transform.rotate = { rotate.x,-rotate.y,-rotate.z }; // x軸を反転、さらに回転方向が逆なので軸を反転させる
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

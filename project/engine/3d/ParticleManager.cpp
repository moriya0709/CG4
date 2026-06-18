#include <cassert>

#include "ParticleManager.h"
#include "TextureManager.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "Camera.h"
#include "CameraManager.h"

std::unique_ptr <ParticleManager> ParticleManager::instance = nullptr;
constexpr uint32_t kMaxParticleInstance = 1024;
// 乱数生成器の初期化
std::random_device seedGenerator;
std::mt19937 randomEngine(seedGenerator());

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::string& directoryPath, const std::string& filename) {
	// 引数で受け取ってメンバ変数に記録する
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	camera_ = Camera::GetInstance();

	// モデル読み込み
	modelData = LoadObjFile(directoryPath, filename);

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
	materialData->enableLighting = false;
	materialData->useNoise = 1;
	materialData->burnColor = Vector3(1.0f, 0.25f, 0.0f);
	materialData->uvTransform = MakeIdentity4x4();
	size_t bufferSize = (sizeof(Material) + 255) & ~255;
	materialResource = dxCommon_->CreateBufferResource(bufferSize);

	// フィールドの設定
	accelerationField.acceleration = { 15.0f,0.0f,0.0f };
	accelerationField.area.min = { -1.0f,-1.0f,-1.0f };
	accelerationField.area.max = { 1.0f,1.0f,1.0f };

	// ルートシグネイチャの作成
	CreateRootSignature();
	// グラフィックスパイプラインの生成
	CreateGraphicsPipeline();
}

void ParticleManager::Update() {
	// 毎フレーム、今アクティブなカメラを取得する
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();

	// 万が一カメラが無い場合は安全のために抜ける
	if (!activeCamera) return;

	Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);

	// 取得した activeCamera からビュー行列をもらう
	Matrix4x4 view = activeCamera->GetViewMatrix();
	view.m[3][0] = 0.0f; view.m[3][1] = 0.0f; view.m[3][2] = 0.0f;
	Matrix4x4 billboardMatrix = Inverse(view);

	for (auto& groupPair : particleGroups) {
		ParticleGroup& group = groupPair.second;

		if (group.particles.empty()) continue;

		ParticleForGPU* mapped = nullptr;
		group.instancingResource->Map(0, nullptr, (void**)&mapped);

		uint32_t index = 0;
		for (auto it = group.particles.begin(); it != group.particles.end(); ) {
			if (it->currentTime >= it->lifeTime) {
				it = group.particles.erase(it);
				continue;
			}

			if (index >= kMaxParticleInstance) {
				break;
			}

			if (useField) {
				if (IsCollision(accelerationField.area, it->transform.translate)) {
					it->velocity += accelerationField.acceleration * kDeltaTime;
				}
			}

			it->transform.translate += it->velocity * kDeltaTime;

			// 色を徐々に変化させる
			float progress = (it->currentTime / it->lifeTime) * it->colorChangeSpeed; // ← it-> に変更
			if (progress > 1.0f) {
				progress = 1.0f;
			}
			if (it->isColorChange[0]) { // ← it-> に変更
				it->color.x = it->startColor.x + (it->finalColor.x - it->startColor.x) * progress; // ← it-> に変更
			}
			if (it->isColorChange[1]) {
				it->color.y = it->startColor.y + (it->finalColor.y - it->startColor.y) * progress;
			}
			if (it->isColorChange[2]) {
				it->color.z = it->startColor.z + (it->finalColor.z - it->startColor.z) * progress;
			}
			if (it->isColorChange[3]) {
				it->color.w = it->startColor.w + (it->finalColor.w - it->startColor.w) * progress;
			}

			// サイズを徐々に変化させる
			if (it->isScaleChange[0]) {
				it->transform.scale.x += it->scaleAdd;

				if (it->transform.scale.x <= 0.0f)
					it->transform.scale.x = 0.0f;
			}
			if (it->isScaleChange[1]) {
				it->transform.scale.y += it->scaleAdd;

				if (it->transform.scale.y <= 0.0f)
					it->transform.scale.y = 0.0f;
			}
			if (it->isScaleChange[2]) {
				it->transform.scale.z += it->scaleAdd;

				if (it->transform.scale.z <= 0.0f)
					it->transform.scale.z = 0.0f;
			}

			// UVスクロール
			it->uvOffset.x += it->uvScrollSpeed.x * kDeltaTime;
			it->uvOffset.y += it->uvScrollSpeed.y * kDeltaTime;

			// ディゾルブ
			//materialData->dissolveThreshold = it->currentTime / it->lifeTime;

			it->currentTime += kDeltaTime;

			Matrix4x4 scale = MakeScaleMatrix(it->transform.scale);
			Matrix4x4 translate = MakeTranslateMatrix(it->transform.translate);
			Matrix4x4 rotateX = MakeRotateXMatrix(it->transform.rotate.x);
			Matrix4x4 rotateY = MakeRotateYMatrix(it->transform.rotate.y);
			Matrix4x4 rotateZ = MakeRotateZMatrix(it->transform.rotate.z);
			Matrix4x4 rotate;
			if (isBillboard)
				rotate = Multiply(Multiply(rotateX, rotateY), billboardMatrix);
			else
				rotate = Multiply(Multiply(rotateX, rotateY), rotateZ);

			
			Matrix4x4 world = Multiply(Multiply(scale, rotate), translate);

			// 取得した activeCamera からビュープロジェクション行列をもらう
			Matrix4x4 viewProj = Multiply(activeCamera->GetViewMatrix(), activeCamera->GetProjectionMatrix());

			mapped[index].world = world;
			mapped[index].WVP = Multiply(world, viewProj);
			mapped[index].color.x = it->color.x * it->emissive;
			mapped[index].color.y = it->color.y * it->emissive;
			mapped[index].color.z = it->color.z * it->emissive;
			mapped[index].color.w = it->color.w;
			mapped[index].uvScale = it->uvScale;
			mapped[index].uvOffset = it->uvOffset;

			++index;
			++it;
		}

		group.instancingResource->Unmap(0, nullptr);
	}
}

void ParticleManager::Draw() {
	// ルートシグネイチャを設定
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature.Get());
	// プリミティブポロジーを設定
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ---------------------------------------------------------
	// 1. 【追加】描画対象のグループへのポインタを一時的に配列に集める
	// ---------------------------------------------------------
	std::vector<ParticleGroup*> sortedGroups;
	sortedGroups.reserve(particleGroups.size());

	for (auto& groupPair : particleGroups) {
		// パーティクルが1つ以上ある場合だけ描画対象にする（元の empty チェックをここに移動）
		if (!groupPair.second.particles.empty()) {
			sortedGroups.push_back(&groupPair.second);
		}
	}

	// ---------------------------------------------------------
	// 2. 【追加】priority（優先度）が小さい順（先に描画したい順）にソートする
	// ---------------------------------------------------------
	std::sort(sortedGroups.begin(), sortedGroups.end(), [](const ParticleGroup* a, const ParticleGroup* b) {
		return a->priority < b->priority;
		});

	// ---------------------------------------------------------
	// 3. ソートされた順にパーティクルグループを描画
	// ---------------------------------------------------------
	for (ParticleGroup* groupPtr : sortedGroups) {
		// 参照として受け取ることで、既存のコード（group.〜）をそのまま動かせます
		ParticleGroup& group = *groupPtr;

		// グループが持っている固有の頂点バッファをセットする
		dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &group.vertexBufferView);

		// ★範囲チェックとフォールバックの追加
		uint32_t blendIndex = static_cast<uint32_t>(group.blendMode);
		if (blendIndex >= kCountOfBlendMode) {
			assert(false && "Invalid blend mode specified for particle group.");
			blendIndex = 0;
		}

		// 安全に検証されたインデックスでPSOをセット
		dxCommon_->GetCommandList()->SetPipelineState(graphicsPipelineStates[blendIndex].Get());

		// マテリアルCBufferの場所を設定
		dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(0, group.materialResource->GetGPUVirtualAddress());

		// パーティクル用 StructuredBuffer(SRV) を設定
		dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(group.instancingIndex));

		// SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である。
		dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(group.materialData.textureIndex));

		// ★ 修正2: 描画時の1インスタンスあたりの頂点数を、グループのモデルデータから取得する
		dxCommon_->GetCommandList()->DrawInstanced(static_cast<UINT>(group.modelData.vertices.size()), static_cast<UINT>(group.particles.size()), 0, 0);
	}
}

// ルートシグネチャの作成
void ParticleManager::CreateRootSignature() {
	// DescriptorRange作成
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRange[0].NumDescriptors = 128; // 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // offsetを自動計算

	// DescriptorRangeForInstancing作成
	D3D12_DESCRIPTOR_RANGE DescriptorRangeForInstancing[1] = {};
	DescriptorRangeForInstancing[0].BaseShaderRegister = 0; // 0から始まる
	DescriptorRangeForInstancing[0].NumDescriptors = 1; // 数は1つ
	DescriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	DescriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // offsetを自動計算

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameter作成
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号０とバインド
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VertexShaderで使う
	rootParameters[1].DescriptorTable.pDescriptorRanges = DescriptorRangeForInstancing; // Tableの中身の配列を指定
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(DescriptorRangeForInstancing); // Tableで利用する数
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // Tableで利用する数
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号１を使う

	descriptionRootSignature.pParameters = rootParameters; // ルートパラメーター配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters); // 配列の長さ

	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // 倍リニアフィルター
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0; // レジスタ番号０を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズ「してバイナリにする
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		//Log(reinterpret_cast<char*> (errorBlob->GetBufferPointer()));
		assert(false);
	}
	// バイナリを元に生成
	hr = dxCommon_->GetDevice()->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));

	// InputLayout
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;


	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendStateの設定
	// 全ての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = true; // ブレンドを有効にする
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	// RasiterzerStateの設定
	// カリングしない（裏面も表示させる）
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Shaderをコンパイルする
	vertexShaderBlob = dxCommon_->CompileShader(L"Resource/shaders/Particle.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	pixelShaderBlob = dxCommon_->CompileShader(L"Resource/shaders/Particle.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);
}
// グラフィックスパイプラインの生成
void ParticleManager::CreateGraphicsPipeline() {
	//PSO
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get(); // RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() }; // VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() }; // PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc; // BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState

	// テクスチャの透明な部分を見えなくする設定
	D3D12_DEPTH_STENCIL_DESC particleDepthDesc{};
	particleDepthDesc.DepthEnable = true;
	particleDepthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	particleDepthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	particleDepthDesc.StencilEnable = false;

	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = particleDepthDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定（気にしなくて良い）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// 各ブレンドモードに対してPSOを作成
	for (int i = 0; i < kCountOfBlendMode; ++i) {
		// i に応じて blendDesc を設定する
		graphicsPipelineStateDesc.BlendState = GetBlendDesc((BlendMode)i);
		dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStates[i]));
	}
}

// .mtlファイルの読み込み
MaterialData ParticleManager::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	MaterialData materialData; // 構築するMaterialData
	std::string line; // ファイルから読んだ１行を格納するもの
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず聞けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// identifierに大路多処理
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			// 連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}

	}

	return materialData;
}
// .objファイルの読み込み
ModelData ParticleManager::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData; // 構築するModelData
	std::vector<Vector4> positions; //位置
	std::vector<Vector3> normals; // 法線
	std::vector<Vector2> texcoords; //　テクスチャ座標
	std::string line; // ファイルから読んだ1行を格納するもの

	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず開けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; // 先頭の識別子を読む

		// identifierに応じた処理
		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			position.x *= -1.0f;
			positions.push_back(position);
		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoords.push_back(texcoord);
		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);
		} else if (identifier == "f") {
			VertexData triangle[3];
			// 面は三角形限定。その他は未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;
				// 頂点の要素へのIndexは「位置/UV/法線」で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/'); // 区切りでインデックスを読んでいく
					elementIndices[element] = std::stoi(index);
				}
				// 要素へのIndexから、実際の要素の値を取得して、頂点を構成する
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];
				texcoord.y = 1.0f - texcoord.y;
				triangle[faceVertex] = { position,texcoord,normal };
			}

			// 頂点を逆順で登録することで、回り順を逆にする
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		} else if (identifier == "mtllib") {
			// materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			// 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}

	return modelData;
}

Particle ParticleManager::MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate) {
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
	Particle particle;
	particle.transform.scale = { 1.0f,1.0f,1.0f };
	particle.transform.rotate = { 0.0f,0.0f,0.0f };
	Vector3 randomTranslate{ distribution(randomEngine),distribution(randomEngine),distribution(randomEngine) };
	particle.transform.translate = translate + randomTranslate;
	particle.velocity = { distribution(randomEngine),distribution(randomEngine),distribution(randomEngine) };

	// 色
	std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
	particle.color = { distColor(randomEngine),distColor(randomEngine),distColor(randomEngine),1.0f };

	// ランダムに1~3秒の間生存するようにする
	std::uniform_real_distribution<float> distTime(1.0f, 3.0f);
	particle.lifeTime = distTime(randomEngine);
	particle.currentTime = 0.0f;

	return particle;
}

Particle ParticleManager::MakeNewParticleEditor(
	std::mt19937& randomEngine,
	const Vector3& translate,
	const Vector3& scale,
	const Vector3& rotate,
	std::uniform_real_distribution<float> distPosition,
	std::uniform_real_distribution<float>distScale,
	std::uniform_real_distribution<float>distRotate,
	std::uniform_real_distribution<float> distVelocity,
	std::uniform_real_distribution<float> distTime,
	bool isRandPosition[3], bool isRandScale[3],
	bool isRandRotate[3], bool isRandVelocity[3], Vector4 color,
	float emissive, Vector4 finalColor, float colorChangeSpeed,
	bool isColorChange[4], bool isScaleChange[3], 
	float scaleAdd, Vector2 uvScale,Vector2 uvScrollSpeed, 
	Vector2 uvOffset
) {
	Particle particle;
	
	// ランダム
	// 座標
	Vector3 randomPosition{ distPosition(randomEngine),distPosition(randomEngine),distPosition(randomEngine) };
	randomPosition.x *= isRandPosition[0];
	randomPosition.y *= isRandPosition[1];
	randomPosition.z *= isRandPosition[2];
	particle.transform.translate = translate + randomPosition;
	// スケール
	Vector3 randomScale{ distScale(randomEngine),distScale(randomEngine),distScale(randomEngine) };
	randomScale.x *= isRandScale[0];
	randomScale.y *= isRandScale[1];
	randomScale.z *= isRandScale[2];
	particle.transform.scale = scale + randomScale;
	// 回転
	Vector3 randomRotate{ distRotate(randomEngine),distRotate(randomEngine),distRotate(randomEngine) };
	randomRotate.x *= isRandRotate[0];
	randomRotate.y *= isRandRotate[1];
	randomRotate.z *= isRandRotate[2];
	particle.transform.rotate = rotate + randomRotate;

	// 速度
	particle.velocity = { distVelocity(randomEngine),distVelocity(randomEngine),distVelocity(randomEngine) };
	particle.velocity.x *= isRandVelocity[0];
	particle.velocity.y *= isRandVelocity[1];
	particle.velocity.z *= isRandVelocity[2];

	// 色
	particle.color = color;
	particle.startColor = color;
	particle.emissive = emissive;

	// ランダムに1~3秒の間生存するようにする
	particle.lifeTime = distTime(randomEngine);
	particle.currentTime = 0.0f;

	// 色の最終値
	particle.finalColor = finalColor;
	// 色の変化速度
	particle.colorChangeSpeed = colorChangeSpeed;
	// 色の変化をするかどうか(0または1)
	particle.isColorChange[0] = isColorChange[0];
	particle.isColorChange[1] = isColorChange[1];
	particle.isColorChange[2] = isColorChange[2];
	particle.isColorChange[3] = isColorChange[3];
	// サイズの変化をするかどうか(0または1)
	particle.isScaleChange[0] = isScaleChange[0];
	particle.isScaleChange[1] = isScaleChange[1];
	particle.isScaleChange[2] = isScaleChange[2];
	// サイズの変化量
	particle.scaleAdd = scaleAdd;
	// UV
	particle.uvScale = uvScale;
	particle.uvOffset = uvOffset;
	particle.uvScrollSpeed = uvScrollSpeed;
	return particle;
}

// パーティクルの発生
void ParticleManager::Emit(
	const std::string& name,
	const Vector3& position,
	const Vector3& scale,
	const Vector3& rotate,
	uint32_t count,
	std::uniform_real_distribution<float> distPosition,
	std::uniform_real_distribution<float>distScale,
	std::uniform_real_distribution<float>distRotate,
	std::uniform_real_distribution<float> distVelocity,
	std::uniform_real_distribution<float> distTime,
	bool isRandPosition[3], bool isRandScale[3],
	bool isRandRotate[3], bool isRandVelocity[3], Vector4 color,
	float emissive, BlendMode blendMode, Vector4 finalColor,
	float colorChangeSpeed, bool isColorChange[4], bool isScaleChange[3],
	float scaleAdd, Vector2 uvScale, Vector2 uvScrollSpeed, 
	Vector2 uvOffset, int32_t useNoise, Vector3 burnColor
) {
	assert(particleGroups.count(name));

	// パーティクルグループを追加
	ParticleGroup& group = particleGroups[name];
	// ブレンドモードを設定
	group.blendMode = blendMode;

	for (uint32_t i = 0; i < count; ++i) {
		if (group.particles.size() >= kMaxParticleInstance) {
			break;
		}

		// ノイズテクスチャ設定
		group.material->useNoise = useNoise;
		group.material->burnColor = burnColor;

		group.particles.push_back(
			MakeNewParticleEditor(randomEngine, position, scale,rotate,
				distPosition, distScale, distRotate, distVelocity, distTime,
				isRandPosition, isRandScale, isRandRotate, isRandVelocity,
				color, emissive, finalColor, colorChangeSpeed,
				isColorChange, isScaleChange, scaleAdd, uvScale, uvScrollSpeed, 
				uvOffset));
	}
}

void ParticleManager::CreateParticleGroup(const std::string& groupName, const std::string& directoryPath, const std::string& filename, const std::string textureFilePath, int priority) {
	// すでに同じ名前のグループがあれば何もしない
	if (particleGroups.contains(groupName)) return;

	ParticleGroup newGroup;
	newGroup.modelData = LoadObjFile(directoryPath, filename);

	// ★ 現在の Initialize 内にある「頂点バッファの作成」「VBVの設定」「マップしてコピー」の処理をここに書く
	newGroup.vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * newGroup.modelData.vertices.size());
	newGroup.vertexBufferView.BufferLocation = newGroup.vertexResource->GetGPUVirtualAddress();
	newGroup.vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * newGroup.modelData.vertices.size());
	newGroup.vertexBufferView.StrideInBytes = sizeof(VertexData);

	VertexData* vertexData = nullptr;
	newGroup.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, newGroup.modelData.vertices.data(), sizeof(VertexData) * newGroup.modelData.vertices.size());
	newGroup.vertexResource->Unmap(0, nullptr);

	// グループごとのマテリアルリソース（CBuffer）を作成・マップする
	newGroup.materialResource = dxCommon_->CreateBufferResource(sizeof(Material));
	newGroup.materialResource->Map(0, nullptr, reinterpret_cast<void**>(&newGroup.material));
	// 初期値の書き込み
	newGroup.material->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	newGroup.material->enableLighting = false;
	newGroup.material->useNoise = 0; // 初期状態ではノイズなし
	newGroup.material->uvTransform = MakeIdentity4x4();
	newGroup.material->burnColor = Vector3(0.0f, 0.0f, 0.0f); // 初期状態では燃えるエフェクトなし

	// マテリアルデータにテクスチャファイルパスを設定
	newGroup.materialData.textureFilePath = textureFilePath;
	// テクスチャの読み込み
	TextureManager::GetInstance()->LoadTexture(newGroup.materialData.textureFilePath);

	// マテリアルデータにテクスチャのSRVインデックスを記録
	newGroup.materialData.textureIndex = TextureManager::GetInstance()->GetSrvIndex(newGroup.materialData.textureFilePath);

	// 優先度の設定
	newGroup.priority = priority;

	// インスタンシング用リソースの生成
	newGroup.instancingResource = dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kMaxParticleInstance);

	// インスタンシング用にSRVを確保してSRVインデックスを記録
	newGroup.instancingIndex = srvManager_->Allocate(1);

	// SRV生成
	srvManager_->CreateSRVforStructuredBuffer(
		newGroup.instancingIndex,
		newGroup.instancingResource.Get(),
		kMaxParticleInstance,
		sizeof(ParticleForGPU));

	// マップに登録
	particleGroups[groupName] = std::move(newGroup);
}

void ParticleManager::CreateParticleGroup(const std::string& groupName, const std::vector<VertexData>& vertices, const std::string textureFilePath, int priority) {
	if (particleGroups.contains(groupName)) return;

	ParticleGroup newGroup;
	newGroup.modelData.vertices = vertices; // 頂点配列をそのまま代入

	// ★ 現在の Initialize 内にある「頂点バッファの作成」「VBVの設定」「マップしてコピー」の処理をここに書く
	newGroup.vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * newGroup.modelData.vertices.size());
	newGroup.vertexBufferView.BufferLocation = newGroup.vertexResource->GetGPUVirtualAddress();
	newGroup.vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * newGroup.modelData.vertices.size());
	newGroup.vertexBufferView.StrideInBytes = sizeof(VertexData);

	VertexData* vertexData = nullptr;
	newGroup.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, newGroup.modelData.vertices.data(), sizeof(VertexData) * newGroup.modelData.vertices.size());
	newGroup.vertexResource->Unmap(0, nullptr);

	// グループごとのマテリアルリソース（CBuffer）を作成・マップする
	newGroup.materialResource = dxCommon_->CreateBufferResource(sizeof(Material));
	newGroup.materialResource->Map(0, nullptr, reinterpret_cast<void**>(&newGroup.material));
	// 初期値の書き込み
	newGroup.material->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	newGroup.material->enableLighting = false;
	newGroup.material->useNoise = 0; // 初期状態ではノイズなし
	newGroup.material->uvTransform = MakeIdentity4x4();
	newGroup.material->burnColor = Vector3(0.0f, 0.0f, 0.0f); // 初期状態では燃えるエフェクトなし

	// マテリアルデータにテクスチャファイルパスを設定
	newGroup.materialData.textureFilePath = textureFilePath;
	// テクスチャの読み込み
	TextureManager::GetInstance()->LoadTexture(newGroup.materialData.textureFilePath);

	// マテリアルデータにテクスチャのSRVインデックスを記録
	newGroup.materialData.textureIndex = TextureManager::GetInstance()->GetSrvIndex(newGroup.materialData.textureFilePath);

	// 優先度を設定
	newGroup.priority = priority;

	// インスタンシング用リソースの生成
	newGroup.instancingResource = dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kMaxParticleInstance);

	// インスタンシング用にSRVを確保してSRVインデックスを記録
	newGroup.instancingIndex = srvManager_->Allocate(1);

	// SRV生成
	srvManager_->CreateSRVforStructuredBuffer(
		newGroup.instancingIndex,
		newGroup.instancingResource.Get(),
		kMaxParticleInstance,
		sizeof(ParticleForGPU));

	// マップに登録
	particleGroups[groupName] = std::move(newGroup);
}

std::vector<VertexData> ParticleManager::Ring() {
	std::vector<VertexData> vertices;

	const uint32_t kRingDivide = 32;
	const float kOuterRadius = 1.0f;
	const float kInnerRadius = 0.2f;
	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kRingDivide);
	const float kUScale = 3.0f;

	for (uint32_t index = 0; index < kRingDivide; ++index) {
		float s = std::sin(index * radianPerDivide);
		float c = std::cos(index * radianPerDivide);
		float sNext = std::sin((index + 1) * radianPerDivide);
		float cNext = std::cos((index + 1) * radianPerDivide);
		float u = (float(index) / float(kRingDivide)) * kUScale;
		float uNext = (float(index + 1) / float(kRingDivide)) * kUScale;

		// 法線はXY平面に対する垂直方向（画面手前）を設定
		Vector3 normal = { 0.0f, 0.0f, -1.0f };

		// ① 外側・現在の頂点
		VertexData v1 = { { -s * kOuterRadius, c * kOuterRadius, 0.0f, 1.0f }, { u, 0.0f }, normal };
		// ② 外側・次の頂点
		VertexData v2 = { { -sNext * kOuterRadius, cNext * kOuterRadius, 0.0f, 1.0f }, { uNext, 0.0f }, normal };
		// ③ 内側・現在の頂点
		VertexData v3 = { { -s * kInnerRadius, c * kInnerRadius, 0.0f, 1.0f }, { u, 1.0f }, normal };
		// ④ 内側・次の頂点
		VertexData v4 = { { -sNext * kInnerRadius, cNext * kInnerRadius, 0.0f, 1.0f }, { uNext, 1.0f }, normal };

		// 1区画につき2つの三角形（合計6頂点）を「時計回り」になるように追加する

		// 三角形1: ① -> ② -> ③
		vertices.push_back(v1);
		vertices.push_back(v2);
		vertices.push_back(v3);

		// 三角形2: ② -> ④ -> ③
		vertices.push_back(v2);
		vertices.push_back(v4);
		vertices.push_back(v3);
	}

	return vertices;
}

std::vector<VertexData> ParticleManager::Cylinder() {
	std::vector<VertexData> vertices;

	const uint32_t kCyliderDivide = 32;
	const float kTopRadius = 1.0f;
	const float kBottomRadius = 1.0f;
	const float kHeight = 1.0f;
	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kCyliderDivide);

	for (uint32_t index = 0; index < kCyliderDivide; ++index) {
		float sin = std::sin(index * radianPerDivide);
		float cos = std::cos(index * radianPerDivide);
		float sinNext = std::sin((index + 1) * radianPerDivide);
		float cosNext = std::cos((index + 1) * radianPerDivide);
		float u = float(index) / float(kCyliderDivide);
		float uNext = float(index + 1) / float(kCyliderDivide);

		// ★修正: 法線はY成分が0、Z成分がcosになります。
		// （滑らかなライティングにするため、CurrentとNextで法線を分けます）
		Vector3 normalCurrent = { -sin, 0.0f, cos };
		Vector3 normalNext = { -sinNext, 0.0f, cosNext };

		// ① 上面・現在の頂点 (Top-Left)
		VertexData v1 = { { -sin * kTopRadius, kHeight, cos * kTopRadius, 1.0f }, { u, 0.0f }, normalCurrent };
		// ② 上面・次の頂点 (Top-Right)
		VertexData v2 = { { -sinNext * kTopRadius, kHeight, cosNext * kTopRadius, 1.0f }, { uNext, 0.0f }, normalNext };
		// ③ 底面・現在の頂点 (Bottom-Left)
		VertexData v3 = { { -sin * kBottomRadius, 0.0f, cos * kBottomRadius, 1.0f }, { u, 1.0f }, normalCurrent };
		// ④ 底面・次の頂点 (Bottom-Right) ★修正: sinNext, cosNextを使う
		VertexData v4 = { { -sinNext * kBottomRadius, 0.0f, cosNext * kBottomRadius, 1.0f }, { uNext, 1.0f }, normalNext };

		// 1区画につき2つの三角形（合計6頂点）を「時計回り」になるように追加する
		// 三角形1: ① (左上) -> ③ (左下) -> ② (右上)
		vertices.push_back(v1);
		vertices.push_back(v3);
		vertices.push_back(v2);

		// 三角形2: ② (右上) -> ③ (左下) -> ④ (右下)
		vertices.push_back(v2);
		vertices.push_back(v3);
		vertices.push_back(v4);
	}

	return vertices;

}

std::vector<VertexData> ParticleManager::Cone() {
	std::vector<VertexData> vertices;

	const uint32_t kDivide = 32;
	const float kTopRadius = 2.5f;    // 上を広く
	const float kBottomRadius = 0.5f; // 下を狭く
	const float kHeight = 4.0f;       // 竜巻の高さ
	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kDivide);

	for (uint32_t index = 0; index < kDivide; ++index) {
		// ... (Cylinderの実装と同じくsin, cosを計算) ...
		float sin = std::sin(index * radianPerDivide);
		float cos = std::cos(index * radianPerDivide);
		float sinNext = std::sin((index + 1) * radianPerDivide);
		float cosNext = std::cos((index + 1) * radianPerDivide);
		float u = float(index) / float(kDivide);
		float uNext = float(index + 1) / float(kDivide);

		// 法線は傾斜を考慮する必要がありますが、簡易的にはCylinderと同じでも機能します
		Vector3 normalCurrent = { -sin, 0.0f, cos };
		Vector3 normalNext = { -sinNext, 0.0f, cosNext };

		VertexData v1 = { { -sin * kTopRadius, kHeight, cos * kTopRadius, 1.0f }, { u, 0.0f }, normalCurrent };
		VertexData v2 = { { -sinNext * kTopRadius, kHeight, cosNext * kTopRadius, 1.0f }, { uNext, 0.0f }, normalNext };
		VertexData v3 = { { -sin * kBottomRadius, 0.0f, cos * kBottomRadius, 1.0f }, { u, 1.0f }, normalCurrent };
		VertexData v4 = { { -sinNext * kBottomRadius, 0.0f, cosNext * kBottomRadius, 1.0f }, { uNext, 1.0f }, normalNext };

		// 三角形ポリゴンを追加
		vertices.push_back(v1); vertices.push_back(v3); vertices.push_back(v2);
		vertices.push_back(v2); vertices.push_back(v3); vertices.push_back(v4);
	}
	return vertices;
}

std::vector<VertexData> ParticleManager::Sphere() {
	std::vector<VertexData> vertices;

	const uint32_t kLatDivide = 32; // 緯度分割数
	const uint32_t kLonDivide = 32; // 経度分割数
	const float kRadius = 1.0f;     // 半径

	for (uint32_t lat = 0; lat < kLatDivide; ++lat) {
		float lat0 = float(lat) / kLatDivide * std::numbers::pi_v<float>;
		float lat1 = float(lat + 1) / kLatDivide * std::numbers::pi_v<float>;

		for (uint32_t lon = 0; lon < kLonDivide; ++lon) {
			float lon0 = float(lon) / kLonDivide * 2.0f * std::numbers::pi_v<float>;
			float lon1 = float(lon + 1) / kLonDivide * 2.0f * std::numbers::pi_v<float>;

			// 4つの頂点を計算
			// 球面座標系 (x = r*sinθ*cosφ, y = r*cosθ, z = r*sinθ*sinφ)
			auto GetVertex = [&](float lat, float lon) -> VertexData {
				float sinLat = std::sin(lat);
				float cosLat = std::cos(lat);
				float sinLon = std::sin(lon);
				float cosLon = std::cos(lon);

				Vector4 pos = {
					kRadius * sinLat * cosLon,
					kRadius * cosLat,
					kRadius * sinLat * sinLon,
					1.0f
				};
				// 法線は球の中心からのベクトル（正規化済み）
				Vector3 normal = { sinLat * cosLon, cosLat, sinLat * sinLon };
				// UVは経度と緯度をマッピング
				Vector2 texcoord = { lon / (2.0f * std::numbers::pi_v<float>), lat / std::numbers::pi_v<float> };
				return { pos, texcoord, normal };
				};

			VertexData v1 = GetVertex(lat0, lon0);
			VertexData v2 = GetVertex(lat0, lon1);
			VertexData v3 = GetVertex(lat1, lon0);
			VertexData v4 = GetVertex(lat1, lon1);

			// 三角形ポリゴンを追加
			vertices.push_back(v1); vertices.push_back(v3); vertices.push_back(v2);
			vertices.push_back(v2); vertices.push_back(v3); vertices.push_back(v4);
		}
	}
	return vertices;
}

// シングルトンインスタンスの取得
ParticleManager* ParticleManager::GetInstance() {
	if (instance == nullptr) {
		instance = std::make_unique <ParticleManager>();
	}
	return instance.get();
}

// ブレンドモードに応じた D3D12_BLEND_DESC を生成して返す関数
D3D12_BLEND_DESC ParticleManager::GetBlendDesc(BlendMode mode) {
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;

	// 【重要】DirectX12では「0」が無効値なので、最初にすべて安全な値で初期化する
	for (int i = 0; i < 8; ++i) {
		blendDesc.RenderTarget[i].BlendEnable = FALSE;
		blendDesc.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;        // 0はエラーになるため1(ONE)
		blendDesc.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;      // 0はエラーになるため1(ZERO)
		blendDesc.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
		blendDesc.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
		blendDesc.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}

	// 0番目のレンダーターゲットのブレンドを有効化
	blendDesc.RenderTarget[0].BlendEnable = TRUE;

	// --- ここからモード別の設定 ---
	switch (mode) {
	case kBlendModeNormal: // 通常（アルファブレンド）
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	break;

	case kBlendModeAdd:    // 加算（光るエフェクト）
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	break;

	case kBlendModeSubtract: // 減算（影や闇のエフェクト）
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
	break;

	case kBlendModeMultiply: // 乗算（セロハンのようなエフェクト）
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_SRC_COLOR;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	break;

	case kBlendModeScreen:   // スクリーン（反転乗算・明るくなる）
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	break;

	default: // 【追加】kCountOfBlendMode などが渡された場合の保険
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	break;
	}

	return blendDesc;
}
#pragma once
#include <string>
#include <list>
#include <D3d12.h>
#include <wrl.h>
#include <unordered_map>
#include <numbers>
#include <vector>
#include <fstream>
#include <dxcapi.h>
#include <random>

#include "Calc.h"
#include "CommonStructs.h"
#include "Blend.h"

class DirectXCommon;
class SrvManager;
class Camera;

// 乱数生成器
extern std::random_device seedGenerator;
extern std::mt19937 randomEngine;

// パーティクル
struct Particle {
	Transform transform;
	Vector3 velocity;
	Vector4 color;
	Vector4 startColor;
	float lifeTime;
	float currentTime;
	Matrix4x4 wvp;
	Matrix4x4 world;
	float emissive;
	Vector4 finalColor;
	float colorChangeSpeed;
	bool isColorChange[4];
	bool isScaleChange[3];
	float scaleAdd;
	Vector2 uvScale;
	Vector2 uvOffset;       // ★追加: 現在のUVのズレ（移動量）
	Vector2 uvScrollSpeed;  // ★追加: 1秒間に進むUVスクロールの速さ
};
// 場(加速度)
struct AccelerationField {
	Vector3 acceleration; //!< 加速度
	AABB area; //!< 範囲
};
// パーティクル描画用データ(GPU用)
struct ParticleForGPU {
	Matrix4x4 WVP;       // 64バイト
	Matrix4x4 world;     // 64バイト
	Vector4 color;       // 16バイト
	Vector2 uvScale;     // 8バイト
	Vector2 uvOffset;
};

class ParticleManager {
public:
	struct ParticleGroup {
		// 頂点データ・モデルデータ
		ModelData modelData;
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

		// マテリアルデータ（必要に応じて）
		Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
		Material* material = nullptr;
		MaterialData materialData;

		// インスタンシング用のデータ（※現在ParticleManagerにあるものを移動）
		Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;
		ParticleForGPU* instancingData = nullptr;
		uint32_t instanceCount = 0;
		uint32_t  instancingIndex;

		// このグループに属するパーティクルのリスト
		std::list<Particle> particles;
		BlendMode blendMode;

		// 描画順の優先度
		int priority = 100;
	};
	std::unordered_map<std::string, ParticleGroup> particleGroups;


	// 初期化
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::string& directoryPath, const std::string& filename);
	// 更新
	void Update();
	// 描画
	void Draw();

	// ルートシグネイチャの作成
	void CreateRootSignature();
	// グラフィックスパイプラインの生成
	void CreateGraphicsPipeline();

	// .mtlファイルの読み込み
	MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	// .objファイルの読み込み
	ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

	// パーティクル生成関数
	Particle MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate);
	// Particle生成関数(エディタ用)
	Particle MakeNewParticleEditor(
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
		float scaleAdd, Vector2 uvScale, Vector2 uvScrollSpeed,
		Vector2 uvOffset
	);

	// パーティクルの発生
	void Emit(const std::string& name,
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
	);

	// OBJファイルからグループを作成
	void CreateParticleGroup(const std::string& groupName, const std::string& directoryPath, const std::string& filename, const std::string textureFilePath, int priority = 100);
	// 生成した頂点データ(Ringなど)からグループを作成
	void CreateParticleGroup(const std::string& groupName, const std::vector<VertexData>& vertices, const std::string textureFilePath, int priority = 100);

	// リングメッシュ生成
	std::vector<VertexData> Ring();
	// 円柱メッシュ生成
	std::vector<VertexData> Cylinder();
	// 円錐メッシュ生成
	std::vector<VertexData> Cone();
	// 球メッシュ生成
	std::vector<VertexData> Sphere();

	// シングルトンインスタンスの取得
	static ParticleManager* GetInstance();

	ParticleManager() = default;
	~ParticleManager() = default;
	ParticleManager(ParticleManager&) = delete;
	ParticleManager& operator=(ParticleManager&) = delete;

private:
	// シングルトンインスタンス
	static std::unique_ptr <ParticleManager> instance;

	// ルートシグネイチャ
	Microsoft::WRL::ComPtr <ID3D12RootSignature> rootSignature = nullptr;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = nullptr;
	D3D12_BLEND_DESC blendDesc{};
	D3D12_RASTERIZER_DESC rasterizerDesc{};

	// グラフィックスパイプライン
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStates[kCountOfBlendMode];

	// Objファイルのデータ
	ModelData modelData;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;

	// バッファリソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;
	Material* materialData = nullptr;

	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	// 場の情報
	AccelerationField accelerationField;
	// 場の影響
	bool useField = false;

	// デルタタイム(60fps固定)
	const float kDeltaTime = 1.0f / 60.0f;

	// ビルボード化
	bool isBillboard = false;

	// インスタンス数
	const uint32_t kNumMaxInstance = 100;

	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;
	// SrvManagerのポインタ
	SrvManager* srvManager_ = nullptr;
	// カメラのポインタ
	Camera* camera_ = nullptr;

	// ブレンドモードに応じた D3D12_BLEND_DESC を生成して返す関数
	D3D12_BLEND_DESC GetBlendDesc(BlendMode mode);

};


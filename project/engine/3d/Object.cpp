#include "Object.h"
#include "Model.h"
#include "ModelManager.h"
#include "Camera.h"
#include "CameraManager.h"
#include "DirectXCommon.h"

void Object::Initialize(Camera* camera) {
	// 引数で受け取ってメンバ変数に記録する
	dxCommon_ = DirectXCommon::GetInstance();
	// デフォルトカメラをセット
	camera_ = camera;

	
	// *座標変換行列* //
	transformationMatrixResource = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
	// 書き込む為のアドレスを取得
	transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));
	// 単位行列を書き込んでおく
	transformationMatrixData->WVP = MakeIdentity4x4();
	transformationMatrixData->World = MakeIdentity4x4();

	// *平行光源* //

	// リソース
	directionalLightResource = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));
	// 書き込む
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	// 初期値を書き込む
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;
	directionalLightData->isDisplay = false;

	// *環境光* //
	ambientLightResource = dxCommon_->CreateBufferResource(sizeof(AmbientLight));
	ambientLightResource->Map(0, nullptr, reinterpret_cast<void**>(&ambientLightData));
	// 初期値
	ambientLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	ambientLightData->intensity = 1.0f;
	ambientLightData->isDisplay = false;

	// *ポイントライト* //
	pointLightResource = dxCommon_->CreateBufferResource(sizeof(PointLight));
	pointLightResource->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData));
	// 初期値
	pointLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLightData->position = { 1.0f, 1.0f, 1.0f};
	pointLightData->intensity = 1.0f;
	pointLightData->radius = 5.0f;
	pointLightData->isDisplay = false;

	// *スポットライト* //
	spotLightResource = dxCommon_->CreateBufferResource(sizeof(SpotLight));
	spotLightResource->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData));
	// 初期値
	spotLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	spotLightData->position = { 0.0f, 3.0f, 0.0f };
	spotLightData->intensity = 1.0f;
	spotLightData->direction = { 0.0f, 0.0f, 0.0f };
	spotLightData->range = 10.0f;
	spotLightData->innerCone = 1.0f;
	spotLightData->outerCone = 0.0f;
	spotLightData->isDisplay = false;

	// アウトライン
	outlineResource = dxCommon_->CreateBufferResource(sizeof(Outline));
	outlineResource->Map(0, nullptr, reinterpret_cast<void**>(&outlineData));
	outlineData->thickness = 0.01f;
	outlineData->color = {1,0,0,0};

	// カメラ
	viewResource = dxCommon_->CreateBufferResource(sizeof(ViewData));
	viewResource->Map(0, nullptr, reinterpret_cast<void**>(&viewData));

	// モーションブラー
	motionBlurResource = dxCommon_->CreateBufferResource(sizeof(MotionBlur));
	motionBlurResource->Map(0, nullptr, reinterpret_cast<void**>(&motionBlurData));
	motionBlurData->isMotionBlur = false;

	// *Transform* //
	transform = {
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f}
	};
	cameraTransform = {
		{1.0f,1.0f,1.0f},
		{0.3f,0.0f,0.0f},
		{0.0f,4.0f,-10.0f}
	};

}

void Object::Update() {
	// Transformの更新
	camera_ = CameraManager::GetInstance()->GetActiveCamera();
	viewData->cameraPos = camera_->GetTranslate();

	// 新しいWVPを計算する前に、現在のWVPを「過去のWVP」として退避させる
	transformationMatrixData->prevWVP = currentWVP_;

	// 通常通り、現在のワールド行列を計算
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	transformationMatrixData->World = worldMatrix;

	// 現在のWVP行列を計算
	currentWVP_ = Multiply(worldMatrix, camera_->GetViewProjectionMatrix());
	transformationMatrixData->WVP = currentWVP_;

	// 太陽ライト
	if(isSunLight)
	SunLight();
	
}

void Object::Draw() {
	// wvp用とWorld用のCBufferの場所を設定
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());
	// アウトライン
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(3, outlineResource->GetGPUVirtualAddress());
	// 平行光
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(4, directionalLightResource->GetGPUVirtualAddress());
	// 環境光
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(5, ambientLightResource->GetGPUVirtualAddress());
	// ポイントライト
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(6, pointLightResource->GetGPUVirtualAddress());
	// スポットライト
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(7, spotLightResource->GetGPUVirtualAddress());
	// カメラ(ビュー)情報
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(8, viewResource->GetGPUVirtualAddress());
	// モーションブラー
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(9, motionBlurResource->GetGPUVirtualAddress());


	// 3Dモデルが割り当てられていれば描画する
	if (model_) {
		model_->Draw();
	}

}

void Object::SetModel(const std::string& filePath) {
	// モデルを検索してセットする
	model_ = ModelManager::GetInstance()->FindModel(filePath);
}

void Object::SunLight() {
	// ライトの角度
	SetDirectionalLightDirection(RayMarching::GetInstance()->GetSunDir());

	// 1. まず方向ベクトルを正規化する（長さを1.0にする）
// ※お使いのライブラリの関数に合わせてください (例: Normalize, Vector3::Normalize など)
	Vector3 normalizedSunDir = Normalize(directionalLightData->direction);

	// 2. 正規化したベクトルのY成分を高さとして使う
	float sunHeight = -normalizedSunDir.y;

	// （以下はこれまでのコードと同じ）
	float dayFactor = std::clamp(sunHeight * 4.0f, 0.0f, 1.0f);
	float sunsetTime = Smoothstep(0.3f, 0.0f, sunHeight) * Smoothstep(-0.2f, 0.0f, sunHeight);

	// 1. 平行光源 (Directional Light) の計算
	// ※代入先と同じ型（Vector4など）を使用してください
	decltype(directionalLightData->color) daySunColor = { 1.0f,  0.92f, 0.85f, 1.0f };
	decltype(directionalLightData->color) sunsetSunColor = { 1.0f,  0.45f, 0.05f, 1.0f };
	decltype(directionalLightData->color) nightSunColor = { 0.08f, 0.10f, 0.18f, 1.0f };

	decltype(directionalLightData->color) currentSunColor = Lerp(daySunColor, sunsetSunColor, sunsetTime);
	currentSunColor = Lerp(nightSunColor, currentSunColor, dayFactor);

	// バッファに書き込む
	directionalLightData->color = currentSunColor;

	// 2. 環境光 (Ambient Light) の計算
	// 直接光だけでなく、影になる部分も夕焼け色に染めると非常に綺麗になります
	decltype(ambientLightData->color) dayAmbient = { 0.3f,  0.5f,  0.8f,  1.0f }; // 青空
	decltype(ambientLightData->color) sunsetAmbient = { 0.8f,  0.4f,  0.3f,  1.0f }; // 夕空
	decltype(ambientLightData->color) nightAmbient = { 0.02f, 0.02f, 0.05f, 1.0f }; // 夜空

	decltype(ambientLightData->color) currentAmbientColor = Lerp(dayAmbient, sunsetAmbient, sunsetTime);
	currentAmbientColor = Lerp(nightAmbient, currentAmbientColor, dayFactor);

	// バッファに書き込む
	ambientLightData->color = currentAmbientColor;
}

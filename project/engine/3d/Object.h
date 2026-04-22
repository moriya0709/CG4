#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <fstream>

#include <D3d12.h>
#include <cassert>
#include <wrl.h>
#include <dxcapi.h>

#include "Calc.h"
#include "RayMarching.h"
#include "CommonStructs.h"

class Model;
class Camera;
class DirectXCommon;

// 平行光源データ
struct DirectionalLight {
	Vector4 color; // ライトの色
	Vector3 direction; // ライトの向き
	float intensity; // 輝度
	int isDisplay; // 表示するかどうか
};
// 環境光データ
struct AmbientLight {
	Vector4 color; // ライトの色
	float intensity; // 輝度
	int isDisplay; // 表示するかどうか
};
// ポイントライトデータ
struct PointLight {
	Vector4 color;
	Vector3 position;
	float intensity; // 輝度
	float radius; // 有効範囲
	int isDisplay;
};
// スポットライトデータ
struct SpotLight {
	Vector4 color; // 色
	Vector3 position; // 位置
	float intensity; // 輝度
	Vector3 direction; // 向き
	float range;        // 距離減衰用
	float innerCone;    // 内側角度
	float outerCone;    // 外側角度
	int isDisplay;
};
// カメラデータ
struct ViewData {
	Vector3 cameraPos;
	float pad;
};


// アウトラインデータ
struct Outline {
	float thickness; // 太さ
	Vector4 color; // 色
	float padding[3];   // 16バイト合わせ（重要）
};

struct MotionBlur {
	int isMotionBlur;
	float pad[3];
};


class Object {
public:
	// 初期化
	void Initialize(Camera* camera);
	// 更新
	void Update();
	// 描画
	void Draw();

	// setter
	void SetModel(Model* model) { model_ = model; }
	void SetModel(const std::string& filePath);
	void SetScale(const Vector3& scale) { transform.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform.translate = translate; }
	void SetCamera(Camera* camera) { camera_ = camera; }
	void SetOutlineThickness(float thickness) { outlineData->thickness = thickness; }
	void SetOutlineColor(Vector4 color) { outlineData->color = color; }
	
	// *ライト* //

	// 平行光
	void SetDirectionalLight(bool isDisplay) { directionalLightData->isDisplay = isDisplay; }
	void SetDirectionalLightColor(Vector4 color) { directionalLightData->color = color; }
	void SetDirectionalLightDirection(Vector3 direction) { directionalLightData->direction = direction; }
	void SetDirectionalLightIntensity(float intensity) { directionalLightData->intensity = intensity; }
	// 環境光
	void SetAmbientLight(bool isDisplay) { ambientLightData->isDisplay = isDisplay; }
	void SetAmbientLightColor(Vector4 color) { ambientLightData->color = color; }
	void SetAmbientLightIntensity(float intensity) { ambientLightData->intensity = intensity; }
	// ポイントライト
	void SetPointLight(bool isDisplay) { pointLightData->isDisplay = isDisplay; }
	void SetPointLightColor(Vector4 color) { pointLightData->color = color; }
	void SetPointLightPosition(Vector3 position) { pointLightData->position = position; }
	void SetPointLightIntensity(float intensity) { pointLightData->intensity = intensity; }
	// スポットライト
	void SetSpotLight(bool isDisplay) { spotLightData->isDisplay = isDisplay; }
	void SetSpotLightColor(Vector4 color) { spotLightData->color = color; }
	void SetSpotLightPosition(Vector3 position) { spotLightData->position = position; }
	void SetSpotLightDirection(Vector3 direction) { spotLightData->direction = direction; }
	void SetSpotLightRange(float range) { spotLightData->range = range; }
	void SetSpotLightIntensity(float intensity) { spotLightData->intensity = intensity; }
	// モーションブラー
	void SetMotionBlur(bool isMotionBlur) { motionBlurData->isMotionBlur = isMotionBlur; }

	// getter
	const Vector3& GetScale() const { return transform.scale; }
	const Vector3& GetRotate() const { return transform.rotate; }
	const Vector3& GetTranslate() const { return transform.translate; }


private:
	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> ambientLightResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> outlineResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> viewResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> motionBlurResource;

	// バッファリソース内のデータを指すポインタ
	TransformationMatrix* transformationMatrixData = nullptr;
	DirectionalLight* directionalLightData = nullptr;
	AmbientLight* ambientLightData = nullptr;
	PointLight* pointLightData = nullptr;
	SpotLight* spotLightData = nullptr;
	Outline* outlineData = nullptr;
	ViewData* viewData = nullptr;
	MotionBlur* motionBlurData = nullptr;

	// Transform
	Transform transform;
	Transform cameraTransform;

	// 太陽のライティングを適応
	bool isSunLight = true;

	// モーションブラー
	Matrix4x4 currentWVP_;


	// モデル
	Model* model_ = nullptr;
	// カメラ
	Camera* camera_ = nullptr;
	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;

	// 空の色をモデルに反映
	void SunLight();

};


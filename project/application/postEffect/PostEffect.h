#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <dxcapi.h>
#include <memory>

#include "Calc.h"
#include "RayMarching.h"

class DirectXCommon;
class WindowAPI;
class SrvManager;
class Camera;

struct RenderTarget {
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
	D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle{};
};

// C++側の構造体 (PostEffect.h など)
struct EffectData {
	int32_t isInversion; // B0
	int32_t isGrayscale;
	int32_t isRadialBlur;
	int32_t isDistanceFog;

	int32_t isDOF; // B1
	int32_t isHeightFog;
	float intensity;
	float pad0;

	Vector2 blurCenter; // B2 (※プロジェクトの型に合わせてください XMFLOAT2など)
	float blurWidth;
	int32_t blurSamples;

	Vector3 distanceFogColor; // B3
	float distanceFogStart;

	float distanceFogEnd; // B4
	float zNear;
	float zFar;
	float pad1;

	Vector3 heightFogColor; // B5
	float heightFogTop;

	float heightFogBottom; // B6
	float heightFogDensity;
	Vector2 pad2;

	Matrix4x4 matInverseViewProjection; // B7-10

	float focusDistance; // B11
	float focusRange;
	float bokehRadius;
	float pad3;

	// *ブルーム* //
	float bloomThreshold;
	float bloomIntensity;
	float bloomBlurRadius; // ★ ここがC++側に無かったり、順番が違うと以降が全てズレます！
	float pad4;

	// *レンズフレア* //
	int32_t isLensFlare;
	int32_t lensFlareGhostCount;
	float lensFlareGhostDispersal;
	float lensFlareHaloWidth;

	int32_t isACES;
	float caIntensity;
	Vector2 pad5;

	// *モーションブラー* //
	int32_t isMotionBlur; // ★ ズレていると、ここに別のデータ(0)が入り込んでしまいます
	int32_t motionBlurSamples;
	float motionBlurScale;
	float pad6;

};

// 各パスのレンダーターゲットとSRVインデックスをまとめる構造体
struct BloomBuffer {
	RenderTarget lumRenderTarget;
	uint32_t lumSrvIndex;

	RenderTarget blurRenderTarget[2];
	uint32_t blurSrvIndex[2];
};

class PostEffect {
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon, WindowAPI* windowAPI, SrvManager* srvManager);
	// 更新
	void Update(Camera* camera);
	// 描画
	void Draw();

	// 描画前処理
	void PreDraw();
	// 描画後処理
	void PostDraw();

	// 反転
	void SetInversion(bool isInversion) { effectData->isInversion = isInversion; }
	// グレースケール
	void SetGrayscale(bool isGrayscale) { effectData->isGrayscale = isGrayscale; }
	// 放射線ブラー
	void SetRadialBlur(bool isRadialBlur) { effectData->isRadialBlur = isRadialBlur; }
	void SetBlurCenter(const Vector2& center) { effectData->blurCenter = center; }
	void SetBlurWidth(float width) { effectData->blurWidth = width; }
	void SetBlurSamples(int samples) { effectData->blurSamples = samples; }

	// ディスタンスフォグ
	void SetDistanceFog(bool isFog) { effectData->isDistanceFog = isFog; }
	void SetDistanceFogColor(const Vector3& color) { effectData->distanceFogColor = color; }
	void SetDistanceFogStart(float start) { effectData->distanceFogStart = start; }
	void SetDistanceFogEnd(float end) { effectData->distanceFogEnd = end; }
	
	// ハイトフォグ
	void SetHeightFog(bool isFog) { effectData->isHeightFog = isFog; }
	void SetHeightFogColor(const Vector3& color) { effectData->heightFogColor = color; }
	void SetHeightFogTop(float top) { effectData->heightFogTop = top; }
	void SetHeightFogBottom(float bottom) { effectData->heightFogBottom = bottom; }
	void SetHeightFogDensity(float density) { effectData->heightFogDensity = density; }
	void SetInverseViewProjectionMatrix(const Matrix4x4& mat) { effectData->matInverseViewProjection = mat; }
	void HightFogUpdate(Camera* camera); // カメラの位置からハイトフォグ用の逆行列を計算してセットする関数

	// DOF
	void SetDOF(bool isDOF) { effectData->isDOF = isDOF; }
	void SetFocusDistance(float distance) { effectData->focusDistance = distance; }
	void SetFocusRange(float range) { effectData->focusRange = range; }
	void SetBokehRadius(float radius) { effectData->bokehRadius = radius; }

	// ブルーム
	void SetBloomThreshold(float bloomThreshold) {effectData->bloomThreshold = bloomThreshold;}
	void SetBloomIntensity(float bloomIntensity) {effectData->bloomIntensity = bloomIntensity;}
	void SetBloomBlurRadius(float bloomBlurRadius) { effectData->bloomBlurRadius = bloomBlurRadius; }
	// レンズフレア
	void SetLensFlare(bool isLensFlare) { effectData->isLensFlare = isLensFlare; }
	void SetLensFlareGhostCount(int count) { effectData->lensFlareGhostCount = count; }
	void SetLensFlareGhostDispersal(float dispersal) { effectData->lensFlareGhostDispersal = dispersal; }
	void SetLensFlareHaloWidth(float width) { effectData->lensFlareHaloWidth = width; }
	void SetCAIntensity(float intensity) { effectData->caIntensity = intensity; } // 色収差
	void SetIsACES(bool isACES) { effectData->isACES = isACES; } // ACES
	// モーションブラー
	void SetMotionBlur(bool isMotionBlur) { effectData->isMotionBlur = isMotionBlur; }
	void SetMotionBlurSamples(int motionBlurSamples) { effectData->motionBlurSamples = motionBlurSamples;}
	void SetMotionBlurScale(float motionBlurScale) {effectData->motionBlurScale = motionBlurScale;}

	// getter
	float GetLensFlareGhostDispersal() { return effectData->lensFlareGhostDispersal; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(uint32_t index) { return rtvHandles[index]; }
	
	// シングルトンインスタンスの取得
	static PostEffect* GetInstance();

	// 深度バッファを指定の状態に遷移
	void TransitionDepthBuffer(D3D12_RESOURCE_STATES newState);

private:
	// ルートシグネイチャ
	Microsoft::WRL::ComPtr <ID3D12RootSignature> rootSignature = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = nullptr;

	// グラフィックスパイプライン
	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStateFinal;

	// レンダーターゲット
	RenderTarget renderTarget_;
	RenderTarget lumRenderTarget_;     // 高輝度抽出用
	RenderTarget blurRenderTarget_[2]; // ぼかし用（Ping-Pong処理用）
	RenderTarget lensFlareRenderTarget_; // レンズフレア
	RenderTarget velocityRenderTarget_; // ベロシティ
	// ビューポート
	D3D12_VIEWPORT viewport_;
	// シザー矩形
	D3D12_RECT scissorRect_;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

	// クリアカラー
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	D3D12_RESOURCE_STATES currentState_;

	// *エフェクト切り換え用* //
	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> effectResource;
	// エフェクトデータ
	EffectData* effectData = nullptr;

	// index
	uint32_t srvIndex_;
	// PostEffect.h の private メンバ変数などに以下を追加
	uint32_t depthSrvIndex_ = 0;

	// シングルトンインスタンス
	static std::unique_ptr <PostEffect> instance;

	// ブルームのパス数（1/2, 1/4, 1/8 の3段階）
	static const int kBloomPassCount = 3;
	// 3段階分のバッファ配列
	BloomBuffer bloomBuffers_[kBloomPassCount];

	// レンズフレア
	uint32_t lensFlareSrvIndex_ = 0;
	// モーションブラー
	uint32_t velocitySrvIndex_ = 0; // ベロシティバッファ(t6)用

	// ポインター
	DirectXCommon* dxCommon_ = nullptr;
	WindowAPI* windowAPI_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	// レンダーターゲットの生成
	RenderTarget CreateRenderTarget(
		ID3D12Device* device,
		uint32_t width,
		uint32_t height,
		DXGI_FORMAT format,
		const float clearColor[4],
		ID3D12DescriptorHeap* rtvHeap,
		UINT rtvIndex,
		ID3D12DescriptorHeap* srvHeap,
		UINT srvIndex
	);

	// リソースバリアの発行
	void Transition(D3D12_RESOURCE_STATES newState);
	// バックバッファを指定の状態に遷移
	void TransitionBackBuffer(D3D12_RESOURCE_STATES newState);

	// --- リソースの状態を切り替える便利関数 ---
	void TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

	// ルートシグネイチャ生成
	void CreateRootSignature();
	// グラフィックスパイプライン生成
	void CreateGraphicsPipeline();

	// ビューポート初期化
	void InitializeViewport();
	// シザリング矩形初期化
	void InitializeScissorRect();
};


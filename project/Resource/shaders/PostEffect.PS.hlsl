#include "PostEffect.hlsli"

Texture2D<float4> gCurrentTexture : register(t0);
Texture2D<float4> gBloom1Texture : register(t1); // 1/2ぼかし
Texture2D<float4> gBloom2Texture : register(t2); // 1/4ぼかし
Texture2D<float4> gBloom3Texture : register(t3); // 1/8ぼかし
Texture2D<float> gDepthTexture : register(t4); // 深度はt4に移動
Texture2D<float4> gLensFlareTexture : register(t5); // レンズフレア
Texture2D<float2> gVelocityTexture : register(t6); // RGチャンネルのみの想定

SamplerState gSampler : register(s0);

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

struct EffectData
{
    int isInversion; // B0
    int isGrayscale;
    int isRadialBlur;
    int isDistanceFog;

    int isDOF; // B1
    int isHeightFog;
    float intensity;
    float pad0;

    float2 blurCenter; // B2
    float blurWidth;
    int blurSamples;

    float3 distanceFogColor; // B3
    float distanceFogStart;

    float distanceFogEnd; // B4
    float zNear;
    float zFar;
    float pad1;

    // *ハイトフォグ* //
    float3 heightFogColor; // B5
    float heightFogTop;

    float heightFogBottom; // B6
    float heightFogDensity;
    float2 pad2;

    float4x4 matInverseViewProjection; // B7-10

    // *DoF* //
    float focusDistance; // B11
    float focusRange;
    float bokehRadius;
    float pad3;

   // *ブルーム* //
    float bloomThreshold; // 光らせる明るさの閾値
    float bloomIntensity; // ブルームの強さ
    float bloomBlurRadius; // ★新規追加：ブルームのぼかし半径（広がり具合）
    float pad4; // パディングを1つに減らす
    
   // *レンズフレア* //
    int isLensFlare; // レンズフレアのON/OFF
    int lensFlareGhostCount; // ゴーストの数
    float lensFlareGhostDispersal; // ゴーストの広がり具合
    float lensFlareHaloWidth; // ヘイローの大きさ
    
    int isACES; // ACESトーンマッピングのON/OFF
    float caIntensity; // 色収差の強さ (0.001f とかが綺麗)
    float2 pad5; // パディング
    
    // *モーションブラー* //
    int isMotionBlur;
    int motionBlurSamples;
    float motionBlurScale;
    float pad6;
};
ConstantBuffer<EffectData> gEffectData : register(b0);

cbuffer RootConstants : register(b1)
{
    int gPassId; // C++から直接パス番号がねじ込まれる！
};

struct SunAndCloudParam
{
    float4x4 invViewProj;
    float3 cameraPos;
    float time;
    float3 sunDir;
    float cloudCoverage;
    float cloudBottom;
    float cloudTop;
    int isRialLight;
    int isAnimeLight;
    float3 cloudOffset;
    int pad;
};
ConstantBuffer<SunAndCloudParam> gSunCloudData : register(b2);

// -----------------------------------------------------------
// 魔法の道具箱（簡易的なガウスぼかし関数と、虹色を作るスペクトル関数）
// -----------------------------------------------------------
// テクスチャをそのままサンプリングする代わりに、この関数を使うと滑らか（円形）になる
float3 SampleGaussian(Texture2D<float4> tex, SamplerState samp, float2 uv, float2 texelSize, float blurSigma)
{
    float3 result = 0;
    float totalWeight = 0;
    // 3x3の簡易ガウスカーネル
    const float kernel[3] = { 0.227027, 0.316216, 0.070270 };
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float2 offset = float2(x, y) * texelSize * blurSigma;
            float weight = kernel[abs(x)] * kernel[abs(y)];
            result += tex.Sample(samp, uv + offset).rgb * weight;
            totalWeight += weight;
        }
    }
    return result / totalWeight;
}

// 虹色（スペクトル）のグラデーションを作る関数
float3 Spectrum(float t)
{
    float3 r = float3(1.0, 0.0, 0.0); // 赤
    float3 g = float3(0.0, 1.0, 0.0); // 緑
    float3 b = float3(0.0, 0.0, 1.0); // 青
    // コサイン波を組み合わせてスペクトルを作る (t: 0.0～1.0)
    float3 color = cos((t - float3(0.0, 0.5, 1.0)) * 3.14159 * 2.0) * 0.5 + 0.5;
    return color;
}

// ACESトーンマッピング関数（簡易版：Narkowicz ACES）
float3 ACESFitted(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// ★ 非線形・距離依存の色収差サンプリング
//    中心から離れるほど RGB ズレが広がる（実レンズの分散特性）
float3 SampleWithCA(Texture2D<float4> tex, SamplerState samp,
                    float2 uv, float2 toCenter, float caIntensity)
{
    float dist = length(toCenter);
    float caScale = caIntensity * dist * dist * 8.0f; // 二乗則：周辺ほど強く
    float2 caDir = normalize(toCenter + 0.0001f);

    float r = tex.Sample(samp, saturate(uv + caDir * caScale * 1.0f)).r;
    float g = tex.Sample(samp, saturate(uv + caDir * caScale * 0.5f)).g;
    float b = tex.Sample(samp, saturate(uv - caDir * caScale * 0.5f)).b;
    return float3(r, g, b);
}

float4 main(VSOutput input) : SV_TARGET
{
    float4 color = gCurrentTexture.Sample(gSampler, input.uv);
    
    // ==========================================
    // パス1：高輝度抽出
    // ==========================================
    if (gPassId == 1) // ★ gPassId に変更
    {
        float brightness = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
        
        // ★ C++から送られてくる bloomThreshold を使う！
        if (brightness > gEffectData.bloomThreshold)
        {
            // ★ 赤ではなく、元の綺麗な色をそのまま抽出する！
            return color;
        }
        else
        {
            // 閾値以下の暗い部分は黒にする（光らせない）
            return float4(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }

    // ==========================================
    // パス2＆3：ガウスぼかし（X方向 / Y方向）
    // ==========================================
    if (gPassId == 2 || gPassId == 3) // ★ gPassId に変更
    {
        uint width, height;
      // 【修正後】一番ぼけているBloom3からサイズを取得する
        gBloom3Texture.GetDimensions(width, height);
        float2 texelSize = (width > 0 && height > 0) ? (1.0f / float2(width, height)) : float2(0.001f, 0.001f);
       
        // ★ 修正：ここも gPassId を使うように変更
        float2 direction = (gPassId == 2) ? float2(1.0f, 0.0f) : float2(0.0f, 1.0f);

        // バイリニアサンプリングを利用した効率的なウェイト
        float offset[3] = { 0.0, 1.384615, 3.230769 };
        float weight[3] = { 0.227027, 0.316216, 0.070270 };

        float3 result = gCurrentTexture.Sample(gSampler, input.uv).rgb * weight[0];
        for (int i = 1; i < 3; i++)
        {
            // ★ gEffectData.bloomBlurRadius をオフセットに掛け合わせる！
            float2 uvOffset = direction * texelSize * offset[i] * gEffectData.bloomBlurRadius;
            // ★ ここを修正：サンプリング位置を 0.0f〜1.0f の範囲にクランプする
            float2 uvSample1 = saturate(input.uv + uvOffset);
            float2 uvSample2 = saturate(input.uv - uvOffset);
            
            result += gCurrentTexture.Sample(gSampler, uvSample1).rgb * weight[i];
            result += gCurrentTexture.Sample(gSampler, uvSample2).rgb * weight[i];
        }
        return float4(result, 1.0);
    }
    
    // レンズフレア
    if (gPassId == 4)
    {
        // --- 1. 共通の変数定義 ---
        float2 uv = input.uv; // ★エラー対策: texcoord ではなく元の uv に戻す
        float2 toCenter = float2(0.5f, 0.5f) - uv;

        uint width, height;
        gCurrentTexture.GetDimensions(width, height);
        float2 texelSize = (width > 0 && height > 0) ? (1.0f / float2(width, height)) : float2(0.001f, 0.001f);

        // ★エラー対策: ヘイロー（光の輪）用の uvAspect の計算を復活させる
        float aspectRatio = (float) width / (float) height;
        float2 uvAspect = float2((uv.x - 0.5f) * aspectRatio, uv.y - 0.5f);

        float3 result = float3(0.0f, 0.0f, 0.0f);

        // ==========================================
        // ★表現力の強化（CAとサイズの動的制御）
        // ==========================================
        // C++から送られてくる dispersal をそのまま使う
        float dynamicDispersal = gEffectData.lensFlareGhostDispersal;

        // ブレンド用の係数 (lerpFactor) を計算
        float lerpFactor = saturate((dynamicDispersal - 0.2f) / (0.8f - 0.2f));

        // ★エラー対策: 動的CAの変数名を元の caIntensity にする
        float baseCAIntensity = gEffectData.caIntensity;
        float caIntensity = baseCAIntensity * lerp(1.0f, 2.0f, lerpFactor);
        
        // -----------------------------------------------------------
        // 2. ゴーストの生成（サイズとCAを動的に変化）
        // -----------------------------------------------------------
        int numGhosts = gEffectData.lensFlareGhostCount;
        
        if (numGhosts > 0)
        {
           // ★修正1： i=0 は「光源そのものに重なる巨大な塊」になるため、 i=1 から開始する！
            for (int i = 1; i < numGhosts; ++i)
            {
                float2 offset = uv + toCenter * dynamicDispersal * (float) i;
                float dist = length(0.5f - offset);
                float2 caOffset = toCenter * caIntensity * dist;
                
                float baseSigma = 3.0f + (float) i * 1.5f;
                float blurSigma = baseSigma * lerp(1.0f, 1.2f, lerpFactor);
                
                float r = SampleGaussian(gBloom3Texture, gSampler, saturate(offset + caOffset), texelSize, blurSigma).r;
                float g = SampleGaussian(gBloom3Texture, gSampler, saturate(offset), texelSize, blurSigma).g;
                float b = SampleGaussian(gBloom3Texture, gSampler, saturate(offset - caOffset), texelSize, blurSigma).b;
                
                float weight = pow(1.0f - (float(i) / max(1.0f, float(numGhosts))), 3.0f);
                
                // ★修正2：画面の端（光源の外側）に発生する不自然なゴーストを暗くして消す（Vignetteマスク）
                // 画面中央付近(0.2以下)で 1.0(表示)、画面端(0.6以上)で 0.0(非表示) に滑らかにフェードアウトさせます
                float distFromCenter = length(toCenter);
                float vignette = 1.0f - smoothstep(0.2f, 0.6f, distFromCenter);
                
                // ★元のウェイトに vignette を掛け合わせて出力を抑える
                result += float3(r, g, b) * weight * vignette * 0.3f;
            }
        }

        // *ヘイロー* //
        float haloRadius = gEffectData.lensFlareHaloWidth;
        float haloThickness = 0.04f;

        float distToCenter = length(uvAspect);

        float innerEdge = haloRadius - haloThickness;
        float outerEdge = haloRadius + haloThickness;
        float weightHalo = smoothstep(innerEdge, haloRadius, distToCenter)
                         - smoothstep(haloRadius, outerEdge, distToCenter);

        float centerFade = smoothstep(0.0f, 0.15f, distToCenter);
        float edgeFade = smoothstep(0.0f, 0.25f, 1.0f - distToCenter * 1.8f);
        weightHalo = saturate(weightHalo) * centerFade * edgeFade;

        if (weightHalo > 0.001f)
        {
            float2 haloOffsetScalar = haloRadius / max(0.001f, length(toCenter));
            float2 haloSampleUV = saturate(uv + toCenter * haloOffsetScalar);

            // gCurrentTexture から非線形色収差でサンプリング
            float3 haloColor = SampleWithCA(gCurrentTexture, gSampler,
                                            haloSampleUV, toCenter, caIntensity * 0.5f);

            // ★ 改良スペクトル（内側:青紫 → 外側:赤橙）
            float ringT = saturate((distToCenter - innerEdge) / max(0.0001f, outerEdge - innerEdge));
            float3 haloSpectrum = Spectrum(ringT);

            result += haloColor * haloSpectrum * weightHalo * 2.5f;
        }

        return float4(result, 1.0f);
    }

    // *通常描画 ＆ 最終合成* //
    if (gPassId == 0)
    {
        // --- 既存のエフェクト処理 ---
        // モノクロ
        if (gEffectData.isGrayscale)
        {
            float gray = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
            color.rgb = float3(gray, gray, gray);
        }

        // 色反転
        if (gEffectData.isInversion)
        {
            color.rgb = 1.0f - color.rgb;
        }
        
        // 放射状ブラー
        if (gEffectData.isRadialBlur)
        {
            float2 direction = input.uv - gEffectData.blurCenter;
            float4 blurColor = color;
            for (int i = 1; i < gEffectData.blurSamples; i++)
            {
                float2 offset = direction * gEffectData.blurWidth * float(i);
                blurColor += gCurrentTexture.Sample(gSampler, input.uv - offset);
            }
            color = blurColor / float(gEffectData.blurSamples);
        }

        // ディスタンスフォグ
        if (gEffectData.isDistanceFog)
        {
            float depth = gDepthTexture.Sample(gSampler, input.uv);
            float linearDepth = (gEffectData.zNear * gEffectData.zFar) / (gEffectData.zFar - depth * (gEffectData.zFar - gEffectData.zNear));
            float fogFactor = saturate((linearDepth - gEffectData.distanceFogStart) / (gEffectData.distanceFogEnd - gEffectData.distanceFogStart));
            color.rgb = lerp(color.rgb, gEffectData.distanceFogColor, fogFactor);
        }

        // ハイトフォグ
        if (gEffectData.isHeightFog)
        {
            float depth = gDepthTexture.Sample(gSampler, input.uv);
            float2 ndcXY = input.uv * 2.0f - 1.0f;
            ndcXY.y *= -1.0f;
            float4 ndcPos = float4(ndcXY, depth, 1.0f);
            float4 worldPosWithW = mul(gEffectData.matInverseViewProjection, ndcPos);
            float3 worldPos = worldPosWithW.xyz / worldPosWithW.w;

            float heightFactor = saturate((gEffectData.heightFogTop - worldPos.y) / (gEffectData.heightFogTop - gEffectData.heightFogBottom));
            heightFactor = pow(heightFactor, gEffectData.heightFogDensity);
            color.rgb = lerp(color.rgb, gEffectData.heightFogColor, heightFactor);
        }

        // DOF
        if (gEffectData.isDOF)
        {
            float depth = gDepthTexture.Sample(gSampler, input.uv);
            float linearDepth = (gEffectData.zNear * gEffectData.zFar) / (gEffectData.zFar - depth * (gEffectData.zFar - gEffectData.zNear));

            float coc = saturate((abs(linearDepth - gEffectData.focusDistance) - gEffectData.focusRange) / gEffectData.bokehRadius);
            float edgeFade = saturate(input.uv.x * 10.0) * saturate((1.0 - input.uv.x) * 10.0) *
                             saturate(input.uv.y * 10.0) * saturate((1.0 - input.uv.y) * 10.0);
            coc *= edgeFade;
            
            if (coc > 0.0)
            {
                float4 accumColor = 0;
                float totalWeight = 0;
                const int sampleCount = 32;
                const float GOLDEN_ANGLE = 2.39996323;

                for (int i = 0; i < sampleCount; i++)
                {
                    float r = sqrt(float(i) / float(sampleCount));
                    float theta = i * GOLDEN_ANGLE;
                    float2 offset = float2(cos(theta), sin(theta)) * r * coc * 0.02;
                    float2 sampleUV = saturate(input.uv + offset);
                    float4 sampleColor = gCurrentTexture.Sample(gSampler, sampleUV);
                    
                    float weight = dot(sampleColor.rgb, float3(0.299, 0.587, 0.114));
                    weight = pow(weight, 2.0) + 0.1;

                    accumColor += sampleColor * weight;
                    totalWeight += weight;
                }
                color = accumColor / totalWeight;
            }
        }
        
         // =======================================================
    // ★ モーションブラー処理
    // =======================================================
        if (gEffectData.isMotionBlur)
        {
        // 現在のピクセルの速度ベクトルを取得 (RG16Fなどを想定)
            float2 velocity = gVelocityTexture.Sample(gSampler, input.uv).rg;
        
        // 速度のスケール調整（強すぎる場合はここで抑える）
            velocity *= gEffectData.motionBlurScale;

        // 速度が極端に小さい場合は処理をスキップ（軽量化）
            if (length(velocity) > 0.0001f)
            {
            // サンプル1回あたりの移動量
                float2 texelStep = velocity / (float) gEffectData.motionBlurSamples;
            
                float4 accumColor = color;
                float2 currentUV = input.uv;

            // 速度ベクトルの方向に向かって複数回サンプリング
                for (int i = 1; i < gEffectData.motionBlurSamples; ++i)
                {
                    currentUV -= texelStep;
                
                // 画面外のサンプリングを防ぐためのクランプ
                    currentUV = saturate(currentUV);
                
                    accumColor += gCurrentTexture.Sample(gSampler, currentUV);
                }
            
            // 平均化
                color = accumColor / (float) gEffectData.motionBlurSamples;
            }
        }
        
    // 1. ブルームの加算（3つのサイズをすべて重ね合わせる）
        float3 b1 = gBloom1Texture.Sample(gSampler, input.uv).rgb * 1.0f;
        float3 b2 = gBloom2Texture.Sample(gSampler, input.uv).rgb * 0.4f;
        float3 b3 = gBloom3Texture.Sample(gSampler, input.uv).rgb * 0.2f;

        float3 totalBloom = b1 + b2 + b3;

      
    // ブルーム強度を掛けて加算
        color.rgb += totalBloom * gEffectData.bloomIntensity;

    // =======================================================
    // ★★★ ここを追加！ レンズフレアを最終カラーに加算 ★★★
    // =======================================================
        if (gEffectData.isLensFlare)
        {
           // ★ if文の中に移動する！
            float3 lensFlare = gLensFlareTexture.Sample(gSampler, input.uv).rgb;
            color.rgb += lensFlare;
        }

        color.rgb *= gEffectData.intensity;

    // --- 3. ACESトーンマッピングへ ---
        if (gEffectData.isACES)
        {
            color.rgb = ACESFitted(color.rgb);
        }
        else
        {
        // Reinhard (指数トーンマッピング)
            float exposure = 1.0f;
            color.rgb = 1.0f - exp(-color.rgb * exposure);
        }
    }

    return color;
}
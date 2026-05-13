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
    // [16 bytes]
    int isInversion;
    int isGrayscale;
    int isRadialBlur;
    int isDistanceFog;

    // [16 bytes]
    int isDOF;
    int isHeightFog;
    float intensity;
    float pad0;

    // [16 bytes]
    float2 blurCenter;
    float blurWidth;
    int blurSamples;

    // [16 bytes]
    float3 distanceFogColor;
    float distanceFogStart;

    // [16 bytes]
    float distanceFogEnd;
    float zNear;
    float zFar;
    float pad1;

    // [16 bytes]
    float3 heightFogColor;
    float heightFogTop;

    // [16 bytes]
    float heightFogBottom;
    float heightFogDensity;
    float2 pad2;

    // [64 bytes]
    float4x4 matInverseViewProjection;

    // [16 bytes]
    float focusDistance;
    float focusRange;
    float bokehRadius;
    float pad3;

    // [16 bytes]
    float bloomThreshold;
    float bloomIntensity;
    float bloomBlurRadius;
    float pad4;

    // [16 bytes]
    int isLensFlare;
    int lensFlareGhostCount;
    float lensFlareGhostDispersal;
    float lensFlareHaloWidth;

    // [16 bytes]
    int isACES;
    float caIntensity;
    float2 pad5;

    // [16 bytes]
    int isMotionBlur;
    int motionBlurSamples;
    float motionBlurScale;
    float pad6;

    // [16 bytes]
    int isFullScreenCA;
    float fullScreenCAIntensity;
    int isVignette;
    float vignetteIntensity;

    // [16 bytes]
    float3 vignetteColor;
    int isGaussianFilter;

    // [16 bytes]
    float gaussianSigma;
    int isOutline;
    float outlineThreshold;
    float pad7;

    // [16 bytes]
    float4 outlineColor;
    
};
ConstantBuffer<EffectData> gEffectData : register(b0);

cbuffer RootConstants : register(b1)
{
    int gPassId;
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

// ガウシアンフィルタ
float3 ApplyGaussianFilter(Texture2D<float4> tex, SamplerState samp, float2 uv, float2 texelSize, float sigma)
{
    float3 result = 0.0f;
    float totalWeight = 0.0f;

    // シグマが小さすぎる場合は元の色を返す（ゼロ除算防止）
    if (sigma < 0.1f)
    {
        return tex.SampleLevel(samp, uv, 0).rgb;
    }

    // サンプリングの最大半径（シグマの約2.5倍の範囲を取れば、ガウス分布の大部分をカバーできます）
    float maxRadius = sigma * 2.5f;
    float twoSigmaSquare = 2.0f * sigma * sigma;

    // サンプル数（32回ならかなり高品質です。重い場合は 16 や 24 に減らしてください）
    const int SAMPLE_COUNT = 32;
    const float GOLDEN_ANGLE = 2.39996323f; // 黄金角 (ラジアン)

    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        // 円の面積に対して均等にサンプル点を配置
        float r = sqrt((float) i / (float) SAMPLE_COUNT) * maxRadius;
        float theta = i * GOLDEN_ANGLE;

        // XYのオフセットを計算
        float2 offset = float2(cos(theta), sin(theta)) * r * texelSize;

        // 中心からの距離 r に基づくガウス重み計算
        float weight = exp(-(r * r) / twoSigmaSquare);

        result += tex.SampleLevel(samp, saturate(uv + offset), 0).rgb * weight;
        totalWeight += weight;
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

// ACESトーンマッピング関数
float3 ACESFitted(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// 非線形・距離依存の色収差サンプリング
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

// 深度値を線形化（実際の距離に変換）する関数
float LinearizeDepth(float depth, float zNear, float zFar)
{
    // DirectXの一般的なZバッファ(0.0～1.0)を距離に変換
    return (zNear * zFar) / (zFar - depth * (zFar - zNear));
}

// Sobelフィルタを使って深度からエッジ（輪郭）を検出する関数（生深度バージョン）
float DetectEdge(Texture2D<float> depthTex, SamplerState samp, float2 uv, float2 texelSize, float threshold)
{
    float2 offsets[9] =
    {
        float2(-1, -1), float2(0, -1), float2(1, -1),
        float2(-1, 0), float2(0, 0), float2(1, 0),
        float2(-1, 1), float2(0, 1), float2(1, 1)
    };

    float depths[9];
    for (int i = 0; i < 9; ++i)
    {
        // LinearizeDepthを通さず、生のZ値(0.0～1.0)をそのまま使う
        depths[i] = depthTex.SampleLevel(samp, saturate(uv + offsets[i] * texelSize), 0).r;
    }

    float Gx = depths[0] - depths[2] + 2.0f * depths[3] - 2.0f * depths[5] + depths[6] - depths[8];
    float Gy = depths[0] + 2.0f * depths[1] + depths[2] - depths[6] - 2.0f * depths[7] - depths[8];

    float edge = sqrt(Gx * Gx + Gy * Gy);

    // 閾値を超えたら 1.0 (線)、それ以外は 0.0 とする
    return step(threshold, edge);
}

float4 main(VSOutput input) : SV_TARGET
{
    float4 color = gCurrentTexture.Sample(gSampler, input.uv);
    
    // 高輝度抽出
    if (gPassId == 1)
    {
        float brightness = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
        
        if (brightness > gEffectData.bloomThreshold)
        {
            return color;
        }
        else
        {
            // 閾値以下の暗い部分は光らせない
            return float4(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }

    // ガウスぼかし（X方向 / Y方向）
    if (gPassId == 2 || gPassId == 3)
    {
        uint width, height;
      // 【修正後】一番ぼけているBloom3からサイズを取得する
        gBloom3Texture.GetDimensions(width, height);
        float2 texelSize = (width > 0 && height > 0) ? (1.0f / float2(width, height)) : float2(0.001f, 0.001f);
       
        float2 direction = (gPassId == 2) ? float2(1.0f, 0.0f) : float2(0.0f, 1.0f);

        // バイリニアサンプリングを利用した効率的なウェイト
        float offset[3] = { 0.0, 1.384615, 3.230769 };
        float weight[3] = { 0.227027, 0.316216, 0.070270 };

        float3 result = gCurrentTexture.Sample(gSampler, input.uv).rgb * weight[0];
        for (int i = 1; i < 3; i++)
        {
            // gEffectData.bloomBlurRadius をオフセットに掛け合わせる！
            float2 uvOffset = direction * texelSize * offset[i] * gEffectData.bloomBlurRadius;
            // ここを修正：サンプリング位置を 0.0f〜1.0f の範囲にクランプする
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
        
        // 2. ゴーストの生成（サイズとCAを動的に変化）
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
                
                float r = ApplyGaussianFilter(gBloom3Texture, gSampler, saturate(offset + caOffset), texelSize, blurSigma).r;
                float g = ApplyGaussianFilter(gBloom3Texture, gSampler, saturate(offset), texelSize, blurSigma).g;
                float b = ApplyGaussianFilter(gBloom3Texture, gSampler, saturate(offset - caOffset), texelSize, blurSigma).b;
                
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
        // 色収差
        if (gEffectData.isFullScreenCA)
        {
            float2 toCenter = float2(0.5f, 0.5f) - input.uv;
            color.rgb = SampleWithCA(gCurrentTexture, gSampler, input.uv, toCenter, gEffectData.fullScreenCAIntensity);
        }
        
        // ビネット
        if (gEffectData.isVignette)
        {
            // 画面中心からの距離を計算 (中心0.0 ～ 四隅約0.707)
            float dist = distance(input.uv, float2(0.5f, 0.5f));
            
            // 距離0.3〜0.8の範囲で 0.0 → 1.0 になるグラデーションを作成
            float vignetteWeight = smoothstep(0.3f, 0.8f, dist);
            
            // 強さを掛ける
            vignetteWeight *= saturate(gEffectData.vignetteIntensity);

            // 合成
            color.rgb = lerp(color.rgb, gEffectData.vignetteColor, vignetteWeight);
        }
        
        // 画面全体のスムージング
        if (gEffectData.isGaussianFilter)
        {
            uint width, height;
            gCurrentTexture.GetDimensions(width, height);
            float2 texelSize = (width > 0 && height > 0) ? (1.0f / float2(width, height)) : float2(0.001f, 0.001f);
            
            // 画像をぼかす
            color.rgb = ApplyGaussianFilter(gCurrentTexture, gSampler, input.uv, texelSize, gEffectData.gaussianSigma);
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
        
        // モーションブラー
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
        
        // ブルームの加算
        float3 b1 = gBloom1Texture.Sample(gSampler, input.uv).rgb * 1.0f;
        float3 b2 = gBloom2Texture.Sample(gSampler, input.uv).rgb * 0.4f;
        float3 b3 = gBloom3Texture.Sample(gSampler, input.uv).rgb * 0.2f;

        float3 totalBloom = b1 + b2 + b3;

      
        // ブルーム強度を掛けて加算
        color.rgb += totalBloom * gEffectData.bloomIntensity;

        // レンズフレア
        if (gEffectData.isLensFlare)
        {
            float3 lensFlare = gLensFlareTexture.Sample(gSampler, input.uv).rgb;
            color.rgb += lensFlare;
        }

        // 色反転
        if (gEffectData.isInversion)
        {
            color.rgb = 1.0f - color.rgb;
        }
        
        // モノクロ
        if (gEffectData.isGrayscale)
        {
            float gray = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
            color.rgb = float3(gray, gray, gray);
        }
        
      // 深度ベースのアウトライン (本番用・パラメータ検証)
        if (gEffectData.isOutline)
        {
            uint width, height;
            gDepthTexture.GetDimensions(width, height);
            float2 texelSize = (width > 0 && height > 0) ? (1.0f / float2(width, height)) : float2(0.001f, 0.001f);

            // 【テスト】C++からの値を一時的に無視して、確実に線が出る値を強制セットする
            //float testThreshold = 0.0005f; // 非常に小さな閾値
            //float4 testColor = float4(1.0f, 0.0f, 0.0f, 1.0f); // 真っ赤 ＆ 不透明(1.0f)

            // gEffectData.outlineThreshold の代わりに testThreshold を使う
            float edge = DetectEdge(
                gDepthTexture,
                gSampler,
                input.uv,
                texelSize,
                gEffectData.outlineThreshold
            );

            // gEffectData.outlineColor の代わりに testColor を使う
            color.rgb = lerp(color.rgb, gEffectData.outlineColor.rgb, edge * gEffectData.outlineColor.a);
        }

        color.rgb *= gEffectData.intensity;

        // ACESトーンマッピング
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
// ===================================================
// TrailEffect.PS.hlsl
// ===================================================

// ▼▼ hlsli に書いていたものを直接ここに書く ▼▼
struct PSInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD;
    float4 Color : COLOR;
};

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);
// ▲▲ ここまで ▲▲

float4 main(PSInput input) : SV_TARGET
{
    // テクスチャから色を取得
    float4 texColor = tex.Sample(smp, input.UV);
    
    // テクスチャの色 × 頂点カラー
    return texColor * input.Color;
}
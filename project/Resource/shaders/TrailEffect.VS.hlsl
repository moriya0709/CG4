// ===================================================
// TrailEffect.VS.hlsl
// ===================================================

// ▼▼ hlsli に書いていたものを直接ここに書く ▼▼
struct VSInput
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD;
    float4 Color : COLOR;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD;
    float4 Color : COLOR;
};

cbuffer cbuff0 : register(b0)
{
    matrix viewProjection;
};
// ▲▲ ここまで ▲▲

PSInput main(VSInput input)
{
    PSInput output;
    
    // 行列を掛けて画面内座標に変換
    output.Position = mul(float4(input.Position, 1.0f), viewProjection);
    
    output.UV = input.UV;
    output.Color = input.Color;
    
    return output;
}
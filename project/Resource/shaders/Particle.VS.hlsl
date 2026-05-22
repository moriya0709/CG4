#include "Particle.hlsli"

struct ParticleForGPU
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4 color;
    float2 uvScale;
    float2 uvOffset; // ★ padding を uvOffset に変更
};

StructuredBuffer<ParticleForGPU> gParticle : register(t0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    
    // 座標変換
    output.position = mul(input.position, gParticle[instanceId].WVP);
    
    // ★修正: スケールを掛けた後に、オフセット（スクロール移動量）を足す
    output.texcoord = (input.texcoord * gParticle[instanceId].uvScale) + gParticle[instanceId].uvOffset;
    
    // 法線と色
    output.normal = normalize(mul(input.normal, (float32_t3x3) gParticle[instanceId].World));
    output.color = gParticle[instanceId].color;
    
    // ※Particle.hlsli 側に `float32_t2 uv : TEXCOORD1;` が定義されている場合、
    // 値を入れないと警告やエラーになるため、計算済みのtexcoordを入れておきます
    output.uv = output.texcoord;
    
    return output;
}
// ==========================================
// 1. 足りなかった Well 構造体を追加
// ==========================================
struct Well
{
    float32_t4x4 skeletonSpaceMatrix;
    float32_t4x4 skeletonSpaceInverseTransposeMatrix;
};

// ==========================================
// 既存の構造体
// ==========================================
struct Vertex
{
    float32_t4 position;
    float32_t2 texcoord;
    float32_t3 normal;
    float32_t3 outlineNormal;
};

struct VertexInfluence
{
    float32_t4 weight;
    int32_t4 index;
};

struct SkinningInformation
{
    uint32_t numVertices;
};

// ==========================================
// レジスタ
// ==========================================
// palette
StructuredBuffer<Well> gMatrixPalette : register(t0);
// 入力頂点
StructuredBuffer<Vertex> gInputVertices : register(t1);
// 入力インフルエンス
StructuredBuffer<VertexInfluence> gInfluences : register(t2);
// Skinning計算後の頂点データ
RWStructuredBuffer<Vertex> gOutputVertices : register(u0);
// Skinningに関するちょっとした情報
ConstantBuffer<SkinningInformation> gSkinningInformation : register(b0);

[numthreads(1024, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    uint32_t vertexIndex = DTid.x;
    if (vertexIndex < gSkinningInformation.numVertices)
    {
        uint32_t vertexIndex = DTid.x;
        if (vertexIndex < gSkinningInformation.numVertices)
        {
        // 1. 入力頂点をそのまま取得
            Vertex input = gInputVertices[vertexIndex];
        
        // 2. 🛠️ テスト用：計算を全てスキップして、そのまま出力へ代入
            Vertex skinned = input;
        
        // 3. 書き込む
            gOutputVertices[vertexIndex] = skinned;
        }
    }
}
#define float32_t4 float4
#define float32_t4x4 float4x4
#define float32_t3 float3
#define float32_t2 float2

struct VertexShaderOutput {
    float32_t4 position : SV_Position;
    float32_t2 texcoord : TEXCOORD0;
    // ワールド座標系へ変換した法線をPixel Shaderへ渡す
    float32_t3 normal : NORMAL0;
};


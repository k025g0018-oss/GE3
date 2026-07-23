#include "Object3d.hlsli"

#define float32_t4 float4
#define float32_t3 float3
#define int32_t int

struct Material {
    float32_t4 color;
    int32_t enableLighting;
};

// 平行光源の情報
struct DirectionalLight {
    float32_t4 color; // ライトの色
    float32_t3 direction; // ライトが進む方向(単位ベクトル)
    float intensity; // ライトの明るさ
};

///// ----- ここに宣言しなさい ----- /////
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);
ConstantBuffer<Material> gMaterial : register(b0);

// 平行光源をPixel Shaderのb1で受け取る
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    output.color = gMaterial.color;
    
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    output.color = gMaterial.color * textureColor;
    
    return output;
}
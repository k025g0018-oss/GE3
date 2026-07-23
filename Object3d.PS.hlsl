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
    
   // テクスチャから色を取得する
    float32_t4 textureColor =
        gTexture.Sample(gSampler, input.texcoord);

    if (gMaterial.enableLighting != 0)
    {
        // 補間後の法線を再度正規化する
        float32_t3 normal = normalize(input.normal);

        // 法線と光が来る方向の内積から明るさを求める
        float lightingCos = saturate(
            dot(normal, -gDirectionalLight.direction)
        );

        // マテリアル・テクスチャ・光源色・明るさを合成する
        output.color =
            gMaterial.color *
            textureColor *
            gDirectionalLight.color *
            lightingCos *
            gDirectionalLight.intensity;
    }
    else
    {
        // ライティングしない場合は従来の色計算を行う
        output.color =
            gMaterial.color *
            textureColor;
    }
    
    return output;
}
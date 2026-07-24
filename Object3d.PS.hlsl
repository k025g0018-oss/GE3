#include "Object3d.hlsli"

#define float32_t4 float4
#define float32_t3 float3
#define int32_t int

struct Material {
    float32_t4 color;
    int32_t enableLighting;
    
    // 0: Lambert、1: Half Lambert
    int32_t lightingMode;
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

        // 法線とライト側への方向の内積を求める
        float NdotL = dot(normal, -gDirectionalLight.direction);

        float lightingCos;
        if (gMaterial.lightingMode == 0)
        {
            // Lambertでは、裏側の明るさが負にならないように0以上へ制限する
            lightingCos = saturate(NdotL);
        }
        else
        {
            // Half Lambertでは内積を[0, 1]へ変換し、2乗して陰影を調整する
            lightingCos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        }
        
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

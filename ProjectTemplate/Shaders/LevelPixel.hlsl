
struct OBJ_ATTRIBUTES
{
	float3      Kd; // diffuse reflectivity
	float	    d; // dissolve (transparency) 
	float3      Ks; // specular reflectivity
	float       Ns; // specular exponent
	float3      Ka; // ambient reflectivity
	float       sharpness; // local reflection map sharpness
	float3      Tf; // transmission filter
	float       Ni; // optical density (index of refraction)
	float3      Ke; // emissive reflectivity
	float		illum; // illumination model
};

struct SHADER_MODEL_DATA
{
    float4x4 worldMatrix;
    OBJ_ATTRIBUTES material;
};
// This is how you declare and access a Vulkan storage buffer in HLSL
StructuredBuffer<SHADER_MODEL_DATA> SceneData : register(b1);

cbuffer SHADER_SCENE_DATA : register(b0)
{
	float4 sunDirection, sunColor, sunAmbient, camPos;
	float4x4 viewMatrix, projectionMatrix;
};

float3 LinearToSRGB(float3 x)
{
    x = saturate(x);
    float3 lo = x * 12.92;
    float3 hi = 1.055 * pow(x, 1.0 / 2.4) - 0.055;
    float3 useHi = step(0.0031308, x); // 0 if x < edge, else 1
    return lerp(lo, hi, useHi);
}



[[vk::binding(0,1)]] Texture2D<float4> albedoTexture;
[[vk::binding(1,1)]] Texture2D<float4> normalTexture;
[[vk::binding(2,1)]] Texture2D<float4> roughnessTexture;
[[vk::binding(3,1)]] Texture2D<float4> metallicTexture;
[[vk::binding(4,1)]] SamplerState materialSampler;

float3 SampleNormalTS(float2 uv)
{
    float3 n = normalTexture.Sample(materialSampler, uv).xyz * 2.0 - 1.0;
    return normalize(n);
}

float3 NormalFromMap(float3 posW, float3 nrmW, float2 uv)
{
    float3 N = normalize(nrmW);

    float3 dp1 = ddx(posW);
    float3 dp2 = ddy(posW);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    float3 T = normalize(dp1 * duv2.y - dp2 * duv1.y);
    T = normalize(T - N * dot(N, T));
    float3 B = normalize(cross(N, T)); 

    float3 nTS = SampleNormalTS(uv);
    return normalize(T * nTS.x + B * nTS.y + N * nTS.z);
}

// an ultra simple hlsl pixel shader
float4 main(float4 pos : SV_POSITION, float3 nrm : NORMAL,
            float3 posW : WORLD, float3 uvw : TEXCOORD, uint index : INDEX) : SV_TARGET
{
    float2 uv = uvw.xy;

    float3 albedo = albedoTexture.Sample(materialSampler, uv).rgb;

    float3 Kd = SceneData[index].material.Kd;
    
    float3 Ks = SceneData[index].material.Ks;
    float3 Ka = SceneData[index].material.Ka;
    float3 Ke = SceneData[index].material.Ke;

    float3 N = NormalFromMap(posW, nrm, uv);
    float3 L = normalize(-sunDirection.xyz); 
    float3 V = normalize(camPos.xyz - posW);

    float NdotL = saturate(dot(N, L));
    
    float3 H = normalize(L + V);
    float specPow = max(SceneData[index].material.Ns, 16.0);
    float specTerm = pow(saturate(dot(N, H)), specPow);

    float3 diffuse = (Kd * albedo) * (sunColor.rgb * NdotL);
    float3 specular = (Ks) * (sunColor.rgb * specTerm);

    float3 ambient = sunAmbient.rgb * (Ka * (Kd * albedo));

    float3 color = diffuse + specular + ambient + Ke;
    color = color / (color + 1.0);
    return float4(LinearToSRGB(color), 1);
}
#pragma pack_matrix(row_major)

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


struct V_OUT
{
    float4 pos : SV_POSITION;
    float3 nrm : NORMAL;
    float3 posW : WORLD;
    float3 uvw : TEXCOORD;
    nointerpolation uint index : INDEX;
};

V_OUT main(float3 pos : POSITION, float3 uvw : TEXCOORD,
            float3 nrm : NORMAL, uint index : SV_InstanceID)// : SV_POSITION
{
    float4 posW = mul(float4(pos, 1), SceneData[index].worldMatrix);
    float4 posH = mul(posW, viewMatrix);
    posH = mul(posH, projectionMatrix);
    V_OUT output =
    {
        posH, mul(nrm, SceneData[index].worldMatrix).xyz, posW.xyz, uvw, index
    };
    return output;
}
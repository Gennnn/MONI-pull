struct PS_In
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

[[vk::binding(0,0)]] Texture2D<float4> uiTexture;
[[vk::binding(1,0)]] SamplerState uiSampler;

float4 main(PS_In i) : SV_TARGET
{
    float4 textureCol = uiTexture.Sample(uiSampler, i.uv);
    return textureCol * i.color;
}
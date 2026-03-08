struct VS_In
{
    float2 pos : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct VS_Out
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

[[vk::push_constant]]
cbuffer Push
{
    float2 viewport;
};

VS_Out main(VS_In i)
{
    VS_Out o;
    
    float2 ndc = (i.pos / viewport) * 2.0f - 1.0f;
    
    o.pos = float4(ndc, 0, 1);
    o.uv = i.uv;
    o.color = i.color;
    return o;
}
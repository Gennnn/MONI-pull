struct VS_Out
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

VS_Out main(uint vertId : SV_VertexID)
{
    VS_Out o;
    
    float2 pos;
    float2 uv;
    
    if (vertId == 0)
    {
        pos = float2(-1.0, -1.0);
        uv = float2(0.0, 0.0);
    }
    else if (vertId == 1)
    {
        pos = float2(-1.0, 3.0);
        uv = float2(0.0, 2.0);
    }
    else
    {
        pos = float2(3.0, -1.0);
        uv = float2(2.0, 0.0);
    }
    
    o.pos = float4(pos, 0.0, 1.0);
    o.uv = uv;
    
    return o;
}
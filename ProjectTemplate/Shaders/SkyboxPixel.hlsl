#define TAU 6.28318530718

#define MOVE_SPEED 0.05
#define PULSE_FREQUENCY 0.2

#define LINE_THICKNESS 0.03
#define GLOW_RADIUS 0.2
#define EDGE_SOFTNESS 0.01
#define GLOW_POWER 2.0

#define MIN_LUMINANCE 0.0
#define MAX_LUMINANCE 0.9

#define CORE_INTENSITY 1.5
#define HALO_INTENSITY 0.75

#define CELL_SIZE_X 0.08
#define CELL_SIZE_Y 0.08

#define X_SLANT 0.0
#define Y_SLANT 0.0

#define ROT_X_DEG -30.0
#define ROT_Y_DEG 0.0

#define PERSPECTIVE -1.2
#define USE_DEPTH 1

#define GRID_SCALE_X 1.0
#define GRID_SCALE_Y 1.0
#define GRID_OFFSET_X 0.0
#define GRID_OFFSET_Y 0.0

#define INVERT_X 0
#define INVERT_Y 0

#define DEPTH_AGGRESSION 1.7

struct PushConstants
{
    float4 glowColor;
    float4 backgroundColorAndTime;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

struct PS_In
{
    float2 uv : TEXCOORD0;
};

float3 rotateX(float3 p, float rad)
{
    float c = cos(rad);
    float s = sin(rad);
    return float3(p.x, c * p.y - s * p.z, s * p.y + c * p.z);
}

float3 rotateY(float3 p, float rad)
{
    float c = cos(rad);
    float s = sin(rad);
    
    return float3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
}

float4 main(PS_In i) : SV_TARGET
{
    float time = pc.backgroundColorAndTime.w;
    
    float3 glowColor = pc.glowColor.rgb;
    float3 backgroundColor = pc.backgroundColorAndTime.rgb;
    
    float2 uv = saturate(i.uv);
    
    uv.x = (uv.x - 0.5) / GRID_SCALE_X + 0.5 + GRID_OFFSET_X;
    uv.y = (uv.y - 0.5) / GRID_SCALE_Y + 0.5 + GRID_OFFSET_Y;
    
    if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1)
    {
        return float4(backgroundColor, 1.0);
    }
    
    float radX = radians(ROT_X_DEG);
    float radY = radians(ROT_Y_DEG);
    
    float3 p = float3((uv.x - 0.5) * 2.0, (uv.y - 0.5) * 2.0, 0.0);
    p = rotateX(p, radX);
    p = rotateY(p, radY);
    
    float perspective = 1.0 / (1.0 + p.z * PERSPECTIVE);
    p.xy *= perspective;
    
    float2 perspectiveUV = float2((p.x + 1.0) * 0.5, (p.y + 1.0) * 0.5);
    
    float depth = 1.0;
    if (USE_DEPTH > 0.5)
    {
        depth = saturate(1.0 - DEPTH_AGGRESSION * max(0.0, p.z));
    }
    
    float xOffset = i.uv.y * X_SLANT;
    float yOffset = i.uv.x * Y_SLANT;
    
    float t = time * MOVE_SPEED;
    float u = (INVERT_X > 0.5) ? (perspectiveUV.x - t + xOffset) : (perspectiveUV.x + t + xOffset);
    float v = (INVERT_Y > 0.5) ? (perspectiveUV.y - t + yOffset) : (perspectiveUV.y + t + yOffset);
    
    float2 cell = float2(u / CELL_SIZE_X, v / CELL_SIZE_Y);
    float2 fracCell = frac(cell);
    
    float dx = min(fracCell.x, 1.0 - fracCell.x);
    float dy = min(fracCell.y, 1.0 - fracCell.y);
    
    float d = min(dx, dy);
    float timePulse = time * PULSE_FREQUENCY;
    float phase = frac(timePulse);
    float pulse = 0.5 + 0.5 * cos(phase * TAU);
    //pulse = pow(pulse, 1.5);
    float luminance = lerp(MIN_LUMINANCE, MAX_LUMINANCE, pulse);
    
    float core = 1.0 - smoothstep(LINE_THICKNESS, LINE_THICKNESS + EDGE_SOFTNESS, d);
    float halo = 1.0 - smoothstep(LINE_THICKNESS, GLOW_RADIUS, d);
    halo = pow(saturate(halo), GLOW_POWER);
    
    float3 emissive = glowColor * (CORE_INTENSITY * core + HALO_INTENSITY * halo) * luminance;
    
    float3 outColor = backgroundColor + emissive * depth;
    return float4(outColor, 1.0);
}
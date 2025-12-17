cbuffer MVP : register(b0)
{
    float4x4 mvp;
};

Texture2D albedo : register(t0);
SamplerState samp : register(s0);

struct VSIn
{
    float4 pos : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

PSIn VSMain(VSIn v)
{
    PSIn o;
    o.pos = mul(v.pos, mvp);
    o.color = v.color;
    o.uv = v.uv;
    return o;
}

float4 PSMain(PSIn i) : SV_TARGET
{
    return albedo.Sample(samp, i.uv) * i.color;
}

cbuffer MeshData : register(b0)
{
    row_major float4x4 model;
    row_major float4x4 view;
    row_major float4x4 proj;
    row_major float4x4 norm;
};

struct VSIn
{
    float3 pos : POSITION;
    float3 tint : COLOR;
    float2 uv : TEXCOORD0;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float3 tint : COLOR;
    float2 uv : TEXCOORD0;
};

PSIn VSMain(VSIn v)
{
    PSIn o;
    float3 worldCenter = mul(float4(0, 0, 0, 1), model).xyz;
    float3 camRight = { view[0][0], view[1][0], view[2][0] };
    float3 camUp = { view[0][1], view[1][1], view[2][1] };
    float3 billboardPos = worldCenter
                        + (v.pos.x * camRight * 0.5)
                        + (v.pos.y * camUp * 0.5);
    o.pos = mul(float4(billboardPos, 1.0), mul(view, proj));
    o.uv = v.uv;
    o.tint = v.tint;
    return o;
}

Texture2D lightTex : register(t0);
SamplerState defaultSampler : register(s0);

float4 PSMain(PSIn input) : SV_TARGET
{
    return float4(normalize(input.tint), 1.0) * lightTex.Sample(defaultSampler, input.uv);
}
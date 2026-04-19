// Reference https://learnopengl.com/PBR/IBL/Diffuse-irradiance

struct VSIn
{
    float3 pos : POSITION;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float3 texCoord : TEXCOORD0;
};

cbuffer MeshData : register(b0)
{
    row_major float4x4 view;
    row_major float4x4 proj;
};

PSIn VSMain(VSIn v)
{
    PSIn o;

    float4 clipPos = mul(float4(v.pos, 1.0), mul(view, proj));
    
    o.pos = clipPos.xyww;
    o.texCoord = v.pos;
    return o;
}

TextureCube skybox : register(t0);
SamplerState defaultSampler : register(s0);

float4 PSMain(PSIn input) : SV_TARGET
{
    float3 color = skybox.Sample(defaultSampler, input.texCoord).rgb;
    color = color / (color + float3(1.0, 1.0, 1.0));
    color = pow(color, 1.0 / 2.2);
    return float4(color, 1.0);
}
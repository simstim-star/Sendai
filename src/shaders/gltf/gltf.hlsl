cbuffer MeshData : register(b0)
{
    row_major float4x4 mvp;
};

cbuffer PBRData : register(b1)
{
    float4 baseColorFactor;
    
    // KHR_texture_transform
    float2 uvOffset;
    float2 uvScale;
    float uvRotation;
};

Texture2D albedoTex : register(t0);
SamplerState samp   : register(s0);

struct VSIn
{
    float4 pos   : POSITION;
    float4 color : COLOR;
    float2 uv    : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};

struct PSIn
{
    float4 pos   : SV_POSITION;
    float4 color : COLOR;
    float2 uv    : TEXCOORD0;
};

// Based in https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_transform/README.md#overview
PSIn VSMain(VSIn v)
{
    PSIn o;
    o.pos = mul(v.pos, mvp);
    o.color = v.color;

    float s = sin(uvRotation);
    float c = cos(uvRotation);

    float3x3 uvMatrix = float3x3(
        c * uvScale.x, s * uvScale.x, 0.0,
       -s * uvScale.y, c * uvScale.y, 0.0,
        uvOffset.x, uvOffset.y, 1.0
    );

    o.uv = mul(float3(v.uv, 1.0), uvMatrix).xy;

    return o;
}

float4 PSMain(PSIn i) : SV_TARGET
{
    float4 albedo = albedoTex.Sample(samp, i.uv);
    return albedo * baseColorFactor * i.color;
}
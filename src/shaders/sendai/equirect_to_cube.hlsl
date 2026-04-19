// Reference: https://learnopengl.com/PBR/IBL/Diffuse-irradiance

struct VSIn
{
    float3 pos : POSITION;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float3 localPos : TEXCOORD0;
};

cbuffer SceneData : register(b0)
{
    matrix View;
    matrix Projection;
};

PSIn VSMain(VSIn input)
{
    PSIn output;
    output.localPos = input.pos;
    output.pos = mul(Projection, mul(View, float4(input.pos, 1.0f)));
    return output;
}

Texture2D equirectangularMap : register(t0);
SamplerState defaultSampler : register(s0);

static const float2 invAtan = float2(0.1591, 0.3183);

float2 SampleEquirectangular(float3 v)
{
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    uv.y = 1.0 - uv.y; // DirectX 
    return uv;
}

float4 PSMain(PSIn input) : SV_TARGET
{
    float2 uv = SampleEquirectangular(normalize(input.localPos));
    float3 color = equirectangularMap.Sample(defaultSampler, uv).rgb;
    
    return float4(color, 1.0f);
}
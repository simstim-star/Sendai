// Reference https://learnopengl.com/PBR/IBL/Diffuse-irradiance


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

TextureCube environmentMap : register(t0);
SamplerState bilinearSampler : register(s0);

static const float PI = 3.14159265359;

float4 PSMain(PSIn input) : SV_Target
{
    float3 normal = normalize(input.localPos);
    float3 irradiance = float3(0.0, 0.0, 0.0);
    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    float sampleDelta = 0.025;
    float nrSamples = 0.0;
    
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += environmentMap.Sample(bilinearSampler, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    
    irradiance = PI * irradiance * (1.0 / nrSamples);
    return float4(irradiance, 1.0);
}
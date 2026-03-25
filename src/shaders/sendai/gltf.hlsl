struct Light
{
    float4 position;
    float4 ambient;
    float4 diffuse;
    float4 specular;
};

cbuffer MeshData : register(b0)
{
    row_major float4x4 model;
    row_major float4x4 view;
    row_major float4x4 proj;
    row_major float4x4 normal;
};

cbuffer PBRData : register(b1)
{
    float4 baseColorFactor;
    
    // KHR_texture_transform
    float2 uvOffset;
    float2 uvScale;
    float uvRotation;
};

cbuffer SceneData : register(b2)
{
    Light light;
    float4 viewPos;
    float shininess;
}

Texture2D albedoTex : register(t0);
Texture2D specularTex : register(t1);

SamplerState samp   : register(s0);

struct VSIn
{
    float4 pos   : POSITION;
    float4 color : COLOR;
    float4 norm : NORMAL;
    float2 uv    : TEXCOORD0;
};

struct PSIn
{
    float4 pos   : SV_POSITION;
    float4 fragPos : POSITION;
    float4 norm : NORMAL;
    float4 color : COLOR;
    float2 uv    : TEXCOORD0;
};

// Based in https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_transform/README.md#overview
PSIn VSMain(VSIn v)
{
    PSIn o;
    float4x4 mvp = mul(model, mul(view, proj));
    o.pos = mul(v.pos, mvp);
    o.fragPos = mul(v.pos, model);
    o.norm = v.norm;
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
    float4 ambient = 0.01f * light.ambient;
    
    float4 lightDir = normalize(light.position - i.fragPos);
    float diffuseFactor = max(dot(normalize(i.norm), lightDir), 0.0);
    float4 diffuse = diffuseFactor * light.diffuse;
    
    float4 viewDir = normalize(viewPos - i.fragPos);
    float4 reflectDir = reflect(-lightDir, normalize(i.norm));
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    float4 specular = spec * light.specular;
    
    float4 albedo = albedoTex.Sample(samp, i.uv);
    float4 specSample = specularTex.Sample(samp, i.uv);
    return (albedo * baseColorFactor) * (diffuse + ambient) + (specSample * specular);
}
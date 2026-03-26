// PBR inspired by https://learnopengl.com/PBR/Lighting
// Bindless inspired by https://alextardif.com/Bindless.html
// Tangents inspired by https://github.com/salvatorespoto/gLTFViewer/tree/master/DX12Engine/Source/Shaders

#include "shader_defs.h"

#define TextureSpace space1

static const float PI = 3.14159265359;

float3 getNormalFromMap(float2 texCoords, float3 normal, float4 tangent);
float DistributionGGX(float3 N, float3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(float3 N, float3 V, float3 L, float roughness);
float3 fresnelSchlick(float cosTheta, float3 F0);

struct Light
{
    float3 position;
    float3 color;
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
    float metallicFactor;
    float roughnessFactor;
    float3 emissiveFactor;
    
    // KHR_texture_transform
    float2 uvOffset;
    float2 uvScale;
    float uvRotation;
    
    uint albedoTextureIndex;
    uint normalTextureIndex;
    uint metallicTextureIndex;
    uint aoTextureIndex;
    uint emissiveTextureIndex;
};

cbuffer SceneData : register(b2)
{
    Light lights[PBR_MAX_LIGHT_NUMBER];
    float3 camPos;
};

Texture2D pbrTextures[PBR_N_TEXTURES_DESCRIPTORS] : register(t0, TextureSpace);

SamplerState defaultSampler : register(s0);

struct VSIn
{
    float3 pos : POSITION;
    float3 norm : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float2 uv2 : TEXCOORD1;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float3 fragPos : POSITION;
    float3 norm : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float2 uv2 : TEXCOORD1;
};

PSIn VSMain(VSIn v)
{
    PSIn o;
    float4x4 mvp = mul(model, mul(view, proj));
    o.pos = mul(float4(v.pos, 1.0), mvp);
    o.fragPos = mul(float4(v.pos, 1.0), model).xyz;
    o.norm = mul(float4(v.norm, 0.0), normal).xyz;
    
    // Based in https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_transform/README.md#overview
    float s = sin(uvRotation);
    float c = cos(uvRotation);
    
    float3x3 uvMatrix = float3x3(
        c * uvScale.x, s * uvScale.x, 0.0,
       -s * uvScale.y, c * uvScale.y, 0.0,
        uvOffset.x, uvOffset.y, 1.0
    );
    
    o.uv = mul(float3(v.uv, 1.0), uvMatrix).xy;
    o.uv2 = v.uv2;
    
    o.tangent.xyz = mul(float4(v.tangent.xyz, 0.0), normal).xyz;
    o.tangent.w = v.tangent.w;
    
    return o;
}

float4 PSMain(PSIn input) : SV_TARGET
{
    float3 albedo = baseColorFactor.xyz * pow(pbrTextures[albedoTextureIndex].Sample(defaultSampler, input.uv).rgb, 2.2);
    float metallic = metallicFactor * pbrTextures[metallicTextureIndex].Sample(defaultSampler, input.uv).b;
    float roughness = roughnessFactor * pbrTextures[metallicTextureIndex].Sample(defaultSampler, input.uv).g;
    float ao = pbrTextures[aoTextureIndex].Sample(defaultSampler, input.uv2).r;
    float3 emissive = emissiveFactor.xyz * pbrTextures[emissiveTextureIndex].Sample(defaultSampler, input.uv).rgb;

    input.norm = normalize(input.norm);
    input.tangent.xyz = normalize(input.tangent.xyz);
    float3 N = getNormalFromMap(input.uv, input.norm, input.tangent);
    float3 V = normalize(camPos - input.fragPos);

    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);

    float3 Lo = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < PBR_MAX_LIGHT_NUMBER; ++i)
    {
        float3 L = normalize(lights[i].position - input.fragPos);
        float3 H = normalize(V + L);
        float distance = length(lights[i].position - input.fragPos);
        float attenuation = 1.0 / (distance * distance);
        float3 radiance = lights[i].color * attenuation;

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
           
        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        float3 specular = numerator / denominator;
        
        float3 kS = F;
        float3 kD = float3(1.0, 1.0, 1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    float3 ambient = float3(0.03, 0.03, 0.03) * albedo * ao;
    float3 color = ambient + Lo + emissive;

    color = color / (color + float3(1.0, 1.0, 1.0));
    color = pow(color, 1.0 / 2.2);

    return float4(color, 1.0);
}

float3 getNormalFromMap(float2 texCoords, float3 normal, float4 tangent)
{
    float3 tangentNormal = pbrTextures[normalTextureIndex].Sample(defaultSampler, texCoords).xyz * 2.0 - 1.0;
    float3 N = normalize(normal);
    float3 T = normalize(tangent.xyz);
    
    T = normalize(T - dot(T, N) * N);
    
    float3 B = cross(N, T) * tangent.w;
    float3x3 TBN = float3x3(
        T.x, B.x, N.x,
        T.y, B.y, N.y,
        T.z, B.z, N.z
    );
    return normalize(mul(TBN, tangentNormal));
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
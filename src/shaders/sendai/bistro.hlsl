#include "shader_defs.h"

#define IrradianceSpace space0
#define TextureSpace space1

static const float PI = 3.14159265359;

float3 getNormalFromMap(float2 texCoords, float3 normal, float4 tangent, uint mapIndex);
float DistributionGGX(float3 N, float3 H, float roughness);
float GeometrySmith(float3 N, float3 V, float3 L, float roughness);
float3 fresnelSchlick(float cosTheta, float3 F0);

cbuffer MeshData : register(b0)
{
    row_major float4x4 model;
    row_major float4x4 view;
    row_major float4x4 proj;
    row_major float4x4 normalMatrix;
};

cbuffer PBRData : register(b1)
{
    float4 albedoFactor; 
    float metallicFactor;
    float roughnessFactor;
    float3 emissiveFactor;
    
    // KHR_texture_transform
    float2 uvOffset;
    float2 uvScale;
    float uvRotation;
    
    uint albedoTextureIndex;
    uint normalTextureIndex;
    uint metalRoughTextureIndex;
    uint aoTextureIndex;
    uint emissiveTextureIndex;
    float alphaCutoff;
};

struct Light
{
    float3 position;
    float3 color;
};

cbuffer SceneData : register(b2)
{
    Light lights[PBR_MAX_LIGHT_NUMBER];
    float3 camPos;
};

Texture2D pbrTextures[PBR_N_TEXTURES_DESCRIPTORS] : register(t0, TextureSpace);
SamplerState defaultSampler : register(s0);
TextureCube irradianceMap : register(t0, IrradianceSpace);

struct VSIn
{
    float3 pos : POSITION;
    float3 norm : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float3 fragPos : POSITION;
    float3 norm : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
};

PSIn VSMain(VSIn v)
{
    PSIn o;
    o.pos = mul(float4(v.pos, 1.0), mul(model, mul(view, proj)));
    o.fragPos = mul(float4(v.pos, 1.0), model).xyz;
    o.norm = normalize(mul(float4(v.norm, 0.0), normalMatrix).xyz);
    
    // KHR_texture_transform
    float s = sin(uvRotation);
    float c = cos(uvRotation);
    float3x3 uvMatrix = float3x3(
        c * uvScale.x, s * uvScale.x, 0.0,
       -s * uvScale.y, c * uvScale.y, 0.0,
        uvOffset.x, uvOffset.y, 1.0
    );
    o.uv = mul(float3(v.uv, 1.0), uvMatrix).xy;
    
    o.tangent.xyz = normalize(mul(float4(v.tangent.xyz, 0.0), normalMatrix).xyz);
    o.tangent.w = v.tangent.w;
    
    return o;
}

float4 PSMain(PSIn input) : SV_TARGET
{
    
    float4 albedoSample = pbrTextures[albedoTextureIndex].Sample(defaultSampler, input.uv);
    if (albedoSample.a < alphaCutoff)
        discard;

    float4 mrSample = pbrTextures[metalRoughTextureIndex].Sample(defaultSampler, input.uv);
    
    float3 albedo = albedoFactor.rgb * pow(albedoSample.rgb, 2.2);
    float metallic = metallicFactor * mrSample.b;
    float roughness = roughnessFactor * mrSample.g;

    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);

    float3 N = getNormalFromMap(input.uv, input.norm, input.tangent, normalTextureIndex);
    float3 V = normalize(camPos - input.fragPos);
    float NdotV = max(dot(N, V), 0.0);

    float3 Lo = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < PBR_MAX_LIGHT_NUMBER; ++i)
    {
        float3 L = normalize(lights[i].position - input.fragPos);
        float3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        
        float distance = length(lights[i].position - input.fragPos);
        float attenuation = 1.0 / (distance * distance);
        float3 radiance = lights[i].color * attenuation;

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
            
        float3 numerator = NDF * G * F;
        float denominator = 4.0 * NdotV * NdotL + 0.0001;
        float3 specular = numerator / denominator;
        
        float3 kS = F;
        float3 kD = (float3(1.0, 1.0, 1.0) - kS);
        kD *= (1.0 - metallic); 

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    float ao = pbrTextures[aoTextureIndex].Sample(defaultSampler, input.uv).r;
    
    float3 F_ambient = fresnelSchlick(NdotV, F0);
    float3 kS_ambient = F_ambient;
    float3 kD_ambient = (float3(1.0, 1.0, 1.0) - kS_ambient);
    kD_ambient *= (1.0 - metallic);

    float3 irradiance = irradianceMap.Sample(defaultSampler, N).rgb;
    float3 diffuseAmbient = irradiance * albedo;
    float3 ambient = (kD_ambient * diffuseAmbient) * ao;

    float3 emissive = emissiveFactor * pbrTextures[emissiveTextureIndex].Sample(defaultSampler, input.uv).rgb;
    float3 color = ambient + Lo + emissive;
    
    color = color / (color + float3(1.0, 1.0, 1.0));
    color = pow(color, 1.0 / 2.2);

    return float4(color, albedoSample.a * albedoFactor.a);
}

float3 getNormalFromMap(float2 texCoords, float3 normal, float4 tangent, uint mapIndex)
{
    float3 tangentNormal = pbrTextures[mapIndex].Sample(defaultSampler, texCoords).xyz * 2.0 - 1.0;
    float3 N = normalize(normal);
    float3 T = normalize(tangent.xyz);
    T = normalize(T - dot(T, N) * N);
    float3 B = cross(N, T) * tangent.w;
    return normalize(mul(tangentNormal, float3x3(T, B, N)));
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a2 = pow(roughness * roughness, 2);
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float k)
{
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    
    float ggx2 = GeometrySchlickGGX(NdotV, k);
    float ggx1 = GeometrySchlickGGX(NdotL, k);

    return ggx1 * ggx2;
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
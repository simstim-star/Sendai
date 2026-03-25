// inspired by https://github.com/salvatorespoto/gLTFViewer/blob/master/DX12Engine/Source/Shaders/grid.hlsl

cbuffer MeshData : register(b0)
{
    row_major float4x4 model;
    row_major float4x4 view;
    row_major float4x4 proj;
    row_major float4x4 norm;
};

struct VertexIn
{
    float3 position : POSITION;
};

struct VertexOut
{
    float4 position : SV_POSITION;
};

VertexOut VSMain(VertexIn vIn)
{
    VertexOut vOut;
    float4 worldPos = mul(float4(vIn.position, 1.0f), model);
    float4 viewPos = mul(worldPos, view);
    vOut.position = mul(viewPos, proj);
    return vOut;
}

float4 PSMain(VertexOut vIn) : SV_Target
{
    return float4(0.3f, 0.3f, 0.3f, 1.0f);
}
struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

cbuffer ModelViewProjection : register(b0)
{
    matrix MVP;
};

PSInput VSMain(float4 position : POSITION, float4 color : COLOR)
{
    PSInput result;
    result.position = mul(position, MVP);
    result.color = color;
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}

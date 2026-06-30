struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord: TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

PSInput main(VSInput input)
{
    PSInput output;
    output.position = float4(input.position, 1.0f);
    output.color = float4(input.texcoord, 0.0f, 0.0f);
    return output;
}
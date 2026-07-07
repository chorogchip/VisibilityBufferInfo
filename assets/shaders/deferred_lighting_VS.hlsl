struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

PSInput main(uint vertexID : SV_VertexID)
{
    PSInput output;
    
    float2 pos[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f)
    };

    float2 uv[3] =
    {
        float2(0.0f, 1.0f),
        float2(0.0f, -1.0f),
        float2(2.0f, 1.0f)
    };

    output.position = float4(pos[vertexID], 0.0f, 1.0f);
    output.uv = uv[vertexID];

    return output;
}
struct PSInput
{
    float4 clipPosition : SV_Position;
    nointerpolation uint geometryInstanceID : TEXCOORD0;
};

struct PSOutput
{
    uint2 visibility : SV_Target0;
};

PSOutput main(PSInput input, uint primitiveID : SV_PrimitiveID)
{
    PSOutput output;
    output.visibility = uint2(input.geometryInstanceID, primitiveID);
    return output;
}

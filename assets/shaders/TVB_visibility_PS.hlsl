struct PSInput
{
    float4 position : SV_POSITION;
    nointerpolation uint object_id : OBJECTID;
};

struct PSOutput
{
    uint2 visibility : SV_Target0;
};

PSOutput main(PSInput input, uint triangle_index : SV_PrimitiveID)
{
    PSOutput output;
    output.visibility = uint2(input.object_id, triangle_index);
    return output;
}

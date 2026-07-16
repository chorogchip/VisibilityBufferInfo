#include "common_material.hlsli"
#include "common_input_struct.hlsli"

struct MaterialData
{
    float4 base_color;
};

StructuredBuffer<MaterialData> gMaterials : register(t1);


float4 main(PSInput input) : SV_TARGET
{
    float2 uv = input.texcoord0;
    float3 normal = normalize(input.normal);
    float4 base_color = gMaterials[input.material_index].base_color;
    return float4(apply_workload(uv, ddx(uv), ddy(uv), normal), base_color.a);
}

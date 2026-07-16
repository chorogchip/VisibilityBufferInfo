#include "common_material.hlsli"
#include "common_gbuffer.hlsli"
#include "common_input_struct.hlsli"

struct MaterialData
{
    float4 base_color;
};

StructuredBuffer<MaterialData> gMaterials : register(t1);

GBufferOutput main(PSInput input)
{
    float2 uv = input.texcoord0;
    float3 normal = normalize(input.normal);
    float4 base_color = gMaterials[input.material_index].base_color;
    float4 gbuffer_value = float4(apply_workload(uv, ddx(uv), ddy(uv), normal), base_color.a);
    
    return make_gbuffer_output(gbuffer_value);
}
#include "..\common\bench_tex_sample.hlsli"
#include "..\common\common_material_data.hlsli"
#include "..\common\common_input_struct.hlsli"

StructuredBuffer<MaterialData> gMaterials : register(t1);

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = input.texcoord0;
    float3 normal = normalize(input.normal);
    float4 base_color = gMaterials[input.material_index].base_color;
    return float4(apply_workload(0, uv, ddx(uv), ddy(uv), normal), base_color.a);
}

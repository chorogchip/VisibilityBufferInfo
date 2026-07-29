#ifndef JUNGLE_POINT_SCENE_ABI_HLSLI
#define JUNGLE_POINT_SCENE_ABI_HLSLI

#include "mydonut_scene_abi.hlsli"

struct JunglePointInstanceData
{
    float3 translation;
    uint sourceIndex;
    float4 rotation;
    float3 scale;
    uint pad0;
};

struct JunglePointPrototypeData
{
    row_major float4x4 prototypeLocalTransform;
    row_major float4x4 instancerWorldTransform;
};

float3 JungleRotateByQuaternion(float3 value, float4 quaternion)
{
    const float3 twiceCross = 2.0 * cross(quaternion.xyz, value);
    return value +
        quaternion.w * twiceCross +
        cross(quaternion.xyz, twiceCross);
}

float3 JungleTransformPoint(
    JunglePointInstanceData instance,
    JunglePointPrototypeData prototype,
    float3 value)
{
    float3 transformed = mul(
        float4(value, 1.0),
        prototype.prototypeLocalTransform).xyz;
    transformed *= instance.scale;
    transformed = JungleRotateByQuaternion(
        transformed,
        instance.rotation);
    transformed += instance.translation;
    return mul(
        float4(transformed, 1.0),
        prototype.instancerWorldTransform).xyz;
}

float3 JungleTransformVector(
    JunglePointInstanceData instance,
    JunglePointPrototypeData prototype,
    float3 value)
{
    float3 transformed = mul(
        float4(value, 0.0),
        prototype.prototypeLocalTransform).xyz;
    transformed *= instance.scale;
    transformed = JungleRotateByQuaternion(
        transformed,
        instance.rotation);
    return mul(
        float4(transformed, 0.0),
        prototype.instancerWorldTransform).xyz;
}

#endif

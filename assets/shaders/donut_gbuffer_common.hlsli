#ifndef DONUT_GBUFFER_COMMON_HLSLI
#define DONUT_GBUFFER_COMMON_HLSLI

#pragma pack_matrix(row_major)

#define MATERIAL_SPACE space0
#define MATERIAL_CONSTANT_REGISTER b0
#define MATERIAL_BASE_COLOR_REGISTER t0
#define MATERIAL_METAL_ROUGHNESS_REGISTER t1
#define MATERIAL_NORMAL_REGISTER t2
#define MATERIAL_EMISSIVE_REGISTER t3
#define MATERIAL_OCCLUSION_REGISTER t4
#define MATERIAL_TRANSMISSION_REGISTER t5
#define MATERIAL_OPACITY_REGISTER t6

#define GBUFFER_INPUT_SPACE space1
#define GBUFFER_PUSH_REGISTER b1
#define GBUFFER_INSTANCE_REGISTER t10
#define GBUFFER_VERTEX_REGISTER t11

#define GBUFFER_VIEW_SPACE space2
#define GBUFFER_VIEW_REGISTER b2
#define GBUFFER_MATERIAL_SAMPLER_REGISTER s0

static const uint MaterialFlags_DoubleSided = 0x00000002;
static const uint MaterialFlags_UseMetalRoughOrSpecularTexture = 0x00000004;
static const uint MaterialFlags_UseBaseOrDiffuseTexture = 0x00000008;
static const uint MaterialFlags_UseEmissiveTexture = 0x00000010;
static const uint MaterialFlags_UseNormalTexture = 0x00000020;
static const uint MaterialFlags_UseOcclusionTexture = 0x00000040;
static const uint MaterialFlags_UseTransmissionTexture = 0x00000080;
static const uint MaterialFlags_MetalnessInRedChannel = 0x00000100;
static const uint MaterialFlags_UseOpacityTexture = 0x00000200;

static const uint SizeOfPosition = 12;
static const uint SizeOfTexcoord = 8;
static const uint SizeOfPackedNormal = 4;
static const float DielectricSpecular = 0.04;

struct PlanarViewConstants
{
    float4x4 matWorldToView;
    float4x4 matViewToClip;
    float4x4 matWorldToClip;
    float4x4 matClipToView;
    float4x4 matViewToWorld;
    float4x4 matClipToWorld;
    float4x4 matViewToClipNoOffset;
    float4x4 matWorldToClipNoOffset;
    float4x4 matClipToViewNoOffset;
    float4x4 matClipToWorldNoOffset;
    float2 viewportOrigin;
    float2 viewportSize;
    float2 viewportSizeInv;
    float2 pixelOffset;
    float2 clipToWindowScale;
    float2 clipToWindowBias;
    float2 windowToClipScale;
    float2 windowToClipBias;
    float4 cameraDirectionOrPosition;
};

struct GBufferFillConstants
{
    PlanarViewConstants view;
    PlanarViewConstants viewPrev;
};

struct GBufferPushConstants
{
    uint startInstanceLocation;
    uint startVertexLocation;
    uint positionOffset;
    uint prevPositionOffset;
    uint texCoordOffset;
    uint normalOffset;
    uint tangentOffset;
};

struct InstanceData
{
    uint flags;
    uint firstGeometryInstanceIndex;
    uint firstGeometryIndex;
    uint numGeometries;
    row_major float3x4 transform;
    row_major float3x4 prevTransform;
};

struct MaterialConstants
{
    float3 baseOrDiffuseColor;
    int flags;
    float3 specularColor;
    int materialID;
    float3 emissiveColor;
    int domain;
    float opacity;
    float roughness;
    float metalness;
    float normalTextureScale;
    float occlusionStrength;
    float alphaCutoff;
    float transmissionFactor;
    int baseOrDiffuseTextureIndex;
    int metalRoughOrSpecularTextureIndex;
    int emissiveTextureIndex;
    int normalTextureIndex;
    int occlusionTextureIndex;
    int transmissionTextureIndex;
    int opacityTextureIndex;
    float2 normalTextureTransformScale;
    uint3 padding1;
    float sssScale;
    float3 sssTransmissionColor;
    float sssAnisotropy;
    float3 sssScatteringColor;
    float hairMelanin;
    float3 hairBaseColor;
    float hairMelaninRedness;
    float hairLongitudinalRoughness;
    float hairAzimuthalRoughness;
    float hairIor;
    float hairCuticleAngle;
    float3 hairDiffuseReflectionTint;
    float hairDiffuseReflectionWeight;
};

cbuffer c_GBuffer : register(GBUFFER_VIEW_REGISTER, GBUFFER_VIEW_SPACE)
{
    GBufferFillConstants g_GBuffer;
};

cbuffer c_Material : register(MATERIAL_CONSTANT_REGISTER, MATERIAL_SPACE)
{
    MaterialConstants g_Material;
};

uint HasMaterialFlag(uint flag)
{
    return (uint(g_Material.flags) & flag) != 0;
}

float UnpackR8Snorm(uint value)
{
    int signedValue = int(value << 24) >> 24;
    return clamp(float(signedValue) / 127.0, -1.0, 1.0);
}

float3 UnpackRgb8Snorm(uint value)
{
    return float3(
        UnpackR8Snorm(value),
        UnpackR8Snorm(value >> 8),
        UnpackR8Snorm(value >> 16));
}

float4 UnpackRgba8Snorm(uint value)
{
    return float4(
        UnpackR8Snorm(value),
        UnpackR8Snorm(value >> 8),
        UnpackR8Snorm(value >> 16),
        UnpackR8Snorm(value >> 24));
}

float3 TransformVector(float3x4 transform, float3 value)
{
    return mul(transform, float4(value, 0.0));
}

float3 TransformPoint(float3x4 transform, float3 value)
{
    return mul(transform, float4(value, 1.0));
}

#endif

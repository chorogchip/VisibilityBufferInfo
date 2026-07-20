
#pragma pack_matrix(row_major)

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

#define GBUFFER_SPACE_MATERIAL 0
#define GBUFFER_BINDING_MATERIAL_CONSTANTS 0
#define GBUFFER_BINDING_MATERIAL_DIFFUSE_TEXTURE 0
#define GBUFFER_BINDING_MATERIAL_SPECULAR_TEXTURE 1
#define GBUFFER_BINDING_MATERIAL_NORMAL_TEXTURE 2
#define GBUFFER_BINDING_MATERIAL_EMISSIVE_TEXTURE 3
#define GBUFFER_BINDING_MATERIAL_OCCLUSION_TEXTURE 4
#define GBUFFER_BINDING_MATERIAL_TRANSMISSION_TEXTURE 5
#define GBUFFER_BINDING_MATERIAL_OPACITY_TEXTURE 6

#define GBUFFER_SPACE_INPUT 1
#define GBUFFER_BINDING_PUSH_CONSTANTS 1
#define GBUFFER_BINDING_INSTANCE_BUFFER 10
#define GBUFFER_BINDING_VERTEX_BUFFER 11

#define GBUFFER_SPACE_VIEW 2
#define GBUFFER_BINDING_VIEW_CONSTANTS 2
#define GBUFFER_BINDING_MATERIAL_SAMPLER 0

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


#define MATERIAL_REGISTER_SPACE     GBUFFER_SPACE_MATERIAL
#define MATERIAL_CB_SLOT            GBUFFER_BINDING_MATERIAL_CONSTANTS
#define MATERIAL_DIFFUSE_SLOT       GBUFFER_BINDING_MATERIAL_DIFFUSE_TEXTURE
#define MATERIAL_SPECULAR_SLOT      GBUFFER_BINDING_MATERIAL_SPECULAR_TEXTURE
#define MATERIAL_NORMALS_SLOT       GBUFFER_BINDING_MATERIAL_NORMAL_TEXTURE
#define MATERIAL_EMISSIVE_SLOT      GBUFFER_BINDING_MATERIAL_EMISSIVE_TEXTURE
#define MATERIAL_OCCLUSION_SLOT     GBUFFER_BINDING_MATERIAL_OCCLUSION_TEXTURE
#define MATERIAL_TRANSMISSION_SLOT  GBUFFER_BINDING_MATERIAL_TRANSMISSION_TEXTURE
#define MATERIAL_OPACITY_SLOT       GBUFFER_BINDING_MATERIAL_OPACITY_TEXTURE

#define MATERIAL_SAMPLER_REGISTER_SPACE GBUFFER_SPACE_VIEW
#define MATERIAL_SAMPLER_SLOT           GBUFFER_BINDING_MATERIAL_SAMPLER

#include "donut_scene_material.hlsli"
#include "donut_material_bindings.hlsli"
#include "donut_motion_vectors.hlsli"
#include "donut_forward_vertex.hlsli"
#include "donut_binding_helpers.hlsli"
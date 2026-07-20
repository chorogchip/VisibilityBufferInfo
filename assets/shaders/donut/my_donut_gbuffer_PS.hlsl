
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

#pragma pack_matrix(row_major)

#include "donut_bindless.h"
#include "donut_forward_vertex.hlsli"
#include "donut_gbuffer_cb.h"
#include "donut_binding_helpers.hlsli"
#include "donut_packing.hlsli"

DECLARE_CBUFFER(GBufferFillConstants, c_GBuffer, GBUFFER_BINDING_VIEW_CONSTANTS, GBUFFER_SPACE_VIEW);

void input_assembler(
    in SceneVertex i_vtx,
    in float4 i_instanceMatrix0 : TRANSFORM0,
    in float4 i_instanceMatrix1 : TRANSFORM1,
    in float4 i_instanceMatrix2 : TRANSFORM2,
#if MOTION_VECTORS
    in float4 i_prevInstanceMatrix0 : PREV_TRANSFORM0,
    in float4 i_prevInstanceMatrix1 : PREV_TRANSFORM1,
    in float4 i_prevInstanceMatrix2 : PREV_TRANSFORM2,
#endif
    in uint i_instance : SV_InstanceID,
    out float4 o_position : SV_Position,
    out SceneVertex o_vtx,
    out uint o_instance : INSTANCE
)
{
    float3x4 instanceMatrix = float3x4(i_instanceMatrix0, i_instanceMatrix1, i_instanceMatrix2);

    o_vtx = i_vtx;
    o_vtx.pos = mul(instanceMatrix, float4(i_vtx.pos, 1.0)).xyz;
    o_vtx.normal = mul(instanceMatrix, float4(i_vtx.normal, 0)).xyz;
    o_vtx.tangent.xyz = mul(instanceMatrix, float4(i_vtx.tangent.xyz, 0)).xyz;
    o_vtx.tangent.w = i_vtx.tangent.w;
#if MOTION_VECTORS
    float3x4 prevInstanceMatrix = float3x4(i_prevInstanceMatrix0, i_prevInstanceMatrix1, i_prevInstanceMatrix2);
    o_vtx.prevPos = mul(prevInstanceMatrix, float4(i_vtx.prevPos, 1.0)).xyz;
#else
    o_vtx.prevPos = o_vtx.pos;
#endif

    float4 worldPos = float4(o_vtx.pos, 1.0);
    float4 viewPos = mul(worldPos, c_GBuffer.view.matWorldToView);
    o_position = mul(viewPos, c_GBuffer.view.matViewToClip);
    o_instance = i_instance;
}

// Use a raw buffer on DX11 to avoid adding the StructuredBuffer flag to the instance buffer.
// On DX11, a structured buffer cannot be used as a vertex buffer, and there should be compatibility with other passes.
// On DX12, using a structured buffer results in more optimal code being generated.
#ifdef TARGET_D3D11
ByteAddressBuffer t_Instances : REGISTER_SRV(GBUFFER_BINDING_INSTANCE_BUFFER, GBUFFER_SPACE_INPUT);
#else
StructuredBuffer<InstanceData> t_Instances : REGISTER_SRV(GBUFFER_BINDING_INSTANCE_BUFFER, GBUFFER_SPACE_INPUT);
#endif
ByteAddressBuffer t_Vertices : REGISTER_SRV(GBUFFER_BINDING_VERTEX_BUFFER, GBUFFER_SPACE_INPUT);

DECLARE_PUSH_CONSTANTS(GBufferPushConstants, g_Push, GBUFFER_BINDING_PUSH_CONSTANTS, GBUFFER_SPACE_INPUT);

// Version of the vertex shader that uses buffer loads to read vertex attributes and transforms.
void buffer_loads(
    in uint i_vertex : SV_VertexID,
	in uint i_instance : SV_InstanceID,
    out float4 o_position : SV_Position,
    out SceneVertex o_vtx,
    out uint o_instance : INSTANCE
)
{
    o_instance = i_instance;

    i_instance += g_Push.startInstanceLocation;
    i_vertex += g_Push.startVertexLocation;

#ifdef TARGET_D3D11
    const InstanceData instance = LoadInstanceData(t_Instances, i_instance * c_SizeOfInstanceData);
#else
    const InstanceData instance = t_Instances[i_instance];
#endif

    float3 pos = asfloat(t_Vertices.Load3(g_Push.positionOffset + i_vertex * c_SizeOfPosition));
    float3 prevPos = asfloat(t_Vertices.Load3(g_Push.prevPositionOffset + i_vertex * c_SizeOfPosition));
    float2 texCoord = asfloat(t_Vertices.Load2(g_Push.texCoordOffset + i_vertex * c_SizeOfTexcoord));
    uint packedNormal = t_Vertices.Load(g_Push.normalOffset + i_vertex * c_SizeOfNormal);
    uint packedTangent = t_Vertices.Load(g_Push.tangentOffset + i_vertex * c_SizeOfNormal);
    float3 normal = Unpack_RGB8_SNORM(packedNormal);
    float4 tangent = Unpack_RGBA8_SNORM(packedTangent);

    o_vtx.pos = mul(instance.transform, float4(pos, 1.0)).xyz;
    o_vtx.texCoord = texCoord;
    o_vtx.normal = mul(instance.transform, float4(normal, 0)).xyz;
    o_vtx.tangent.xyz = mul(instance.transform, float4(tangent.xyz, 0)).xyz;
    o_vtx.tangent.w = tangent.w;
#if MOTION_VECTORS
    o_vtx.prevPos = mul(instance.prevTransform, float4(prevPos, 1.0)).xyz;
#else
    o_vtx.prevPos = o_vtx.pos;
#endif

    float4 worldPos = float4(o_vtx.pos, 1.0);
    o_position = mul(worldPos, c_GBuffer.view.matWorldToClip);

}

#pragma once

#include <cstdint>
#include <cstddef>
#include <DirectXMath.h>

namespace scene::dnt {

    struct MaterialConstants {

        DirectX::XMFLOAT3  baseOrDiffuseColor;
        int32_t     flags;

        DirectX::XMFLOAT3  specularColor;
        int32_t     materialID;

        DirectX::XMFLOAT3  emissiveColor;
        int32_t     domain;

        float   opacity;
        float   roughness;
        float   metalness;
        float   normalTextureScale;

        float   occlusionStrength;
        float   alphaCutoff;
        float   transmissionFactor;
        int32_t     baseOrDiffuseTextureIndex;

        int32_t     metalRoughOrSpecularTextureIndex;
        int32_t     emissiveTextureIndex;
        int32_t     normalTextureIndex;
        int32_t     occlusionTextureIndex;

        int32_t     transmissionTextureIndex;
        int32_t     opacityTextureIndex;
        DirectX::XMFLOAT2  normalTextureTransformScale;

        DirectX::XMUINT3   padding1;
        float   sssScale;

        DirectX::XMFLOAT3  sssTransmissionColor;
        float   sssAnisotropy;

        DirectX::XMFLOAT3  sssScatteringColor;
        float   hairMelanin;

        DirectX::XMFLOAT3  hairBaseColor;
        float   hairMelaninRedness;

        float   hairLongitudinalRoughness;
        float   hairAzimuthalRoughness;
        float   hairIor;
        float   hairCuticleAngle;

        DirectX::XMFLOAT3  hairDiffuseReflectionTint;
        float   hairDiffuseReflectionWeight;
    };

    struct GeometryData {
        uint32_t numIndices;
        uint32_t numVertices;
        int32_t indexBufferIndex;
        uint32_t indexOffset;

        int32_t vertexBufferIndex;
        uint32_t positionOffset;
        uint32_t prevPositionOffset;
        uint32_t texCoord1Offset;

        uint32_t texCoord2Offset;
        uint32_t normalOffset;
        uint32_t tangentOffset;
        uint32_t curveRadiusOffset;

        uint32_t materialIndex;
        uint32_t pad0;
        uint32_t pad1;
        uint32_t pad2;
    };

    struct InstanceData {
        uint32_t flags;
        uint32_t firstGeometryInstanceIndex; // index into global list of geometry instances. 
        // foreach (Instance)
        //     foreach(Geo) index++
        uint32_t firstGeometryIndex;         // index into global list of geometries. 
        // foreach(Mesh)
        //     foreach(Geo) index++
        uint32_t numGeometries;

        DirectX::XMFLOAT3X4 transform;
        DirectX::XMFLOAT3X4 prevTransform;
    };

    struct PlanarViewConstants {

        DirectX::XMFLOAT4X4    matWorldToView;
        DirectX::XMFLOAT4X4    matViewToClip;
        DirectX::XMFLOAT4X4    matWorldToClip;
        DirectX::XMFLOAT4X4    matClipToView;
        DirectX::XMFLOAT4X4    matViewToWorld;
        DirectX::XMFLOAT4X4    matClipToWorld;

        DirectX::XMFLOAT4X4    matViewToClipNoOffset;
        DirectX::XMFLOAT4X4    matWorldToClipNoOffset;
        DirectX::XMFLOAT4X4    matClipToViewNoOffset;
        DirectX::XMFLOAT4X4    matClipToWorldNoOffset;

        DirectX::XMFLOAT2      viewportOrigin;
        DirectX::XMFLOAT2      viewportSize;

        DirectX::XMFLOAT2      viewportSizeInv;
        DirectX::XMFLOAT2      pixelOffset;

        DirectX::XMFLOAT2      clipToWindowScale;
        DirectX::XMFLOAT2      clipToWindowBias;

        DirectX::XMFLOAT2      windowToClipScale;
        DirectX::XMFLOAT2      windowToClipBias;

        DirectX::XMFLOAT4      cameraDirectionOrPosition;
    };

    struct GBufferFillConstants {
        PlanarViewConstants view;
        PlanarViewConstants viewPrev;
    };

    struct GBufferPushConstants {
        uint32_t        startInstanceLocation;
        uint32_t        startVertexLocation;
        uint32_t        positionOffset;
        uint32_t        prevPositionOffset;
        uint32_t        texCoordOffset;
        uint32_t        normalOffset;
        uint32_t        tangentOffset;
    };

    static_assert(sizeof(MaterialConstants) == 208);
    static_assert(sizeof(GeometryData) == 64);
    static_assert(sizeof(InstanceData) == 112);
    static_assert(sizeof(GBufferPushConstants) == 28);
    static_assert(offsetof(MaterialConstants, flags) == 12);
    static_assert(offsetof(MaterialConstants, materialID) == 28);
    static_assert(offsetof(MaterialConstants, domain) == 44);
    static_assert(offsetof(MaterialConstants, baseOrDiffuseTextureIndex) == 76);
    static_assert(offsetof(MaterialConstants, metalRoughOrSpecularTextureIndex) == 80);
    static_assert(offsetof(MaterialConstants, normalTextureIndex) == 88);
    static_assert(offsetof(MaterialConstants, opacityTextureIndex) == 100);
    static_assert(offsetof(MaterialConstants, hairDiffuseReflectionWeight) == 204);
    static_assert(offsetof(GeometryData, materialIndex) == 48);
    static_assert(offsetof(GeometryData, positionOffset) == 20);
    static_assert(offsetof(GeometryData, tangentOffset) == 40);
    static_assert(offsetof(InstanceData, transform) == 16);
    static_assert(offsetof(InstanceData, prevTransform) == 64);
}

/*

#ifndef FORWARD_VERTEX_HLSLI
#define FORWARD_VERTEX_HLSLI

struct SceneVertex
{
    float3 pos : POS;
    float3 prevPos : PREV_POS;
    float2 texCoord : TEXCOORD;
    centroid float3 normal : NORMAL;
    centroid float4 tangent : TANGENT;
};

*/


/*
* Copyright (c) 2014-2024, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#pragma pack_matrix(row_major)

#include "..\\donut\\donut_depth_cb.h"
#include "..\\donut\\donut_bindless.h"
#include "..\\donut\\donut_binding_helpers.hlsli"

DECLARE_CBUFFER(DepthPassConstants, g_Depth, DEPTH_BINDING_VIEW_CONSTANTS, DEPTH_SPACE_VIEW);

#ifdef TARGET_D3D11
ByteAddressBuffer t_Instances : REGISTER_SRV(DEPTH_BINDING_INSTANCE_BUFFER, DEPTH_SPACE_INPUT);
#else
StructuredBuffer<InstanceData> t_Instances : REGISTER_SRV(DEPTH_BINDING_INSTANCE_BUFFER, DEPTH_SPACE_INPUT);
#endif
ByteAddressBuffer t_Vertices : REGISTER_SRV(DEPTH_BINDING_VERTEX_BUFFER, DEPTH_SPACE_INPUT);

struct DrawInstanceData
{
    uint instanceID;
    uint submeshID;
};

StructuredBuffer<DrawInstanceData> t_DrawInstances :
    REGISTER_SRV(DEPTH_BINDING_DRAW_INSTANCE_BUFFER, DEPTH_SPACE_INPUT);
StructuredBuffer<uint> t_DrawInstanceIDs :
    REGISTER_SRV(DEPTH_BINDING_DRAW_INSTANCE_ID_BUFFER, DEPTH_SPACE_INPUT);

DECLARE_PUSH_CONSTANTS(
    DepthPushConstants,
    g_Push,
    DEPTH_BINDING_PUSH_CONSTANTS,
    DEPTH_SPACE_INPUT);

// This is the D3D12 manual-vertex-fetch entry point copied from Donut's
// donut_depth_VS.hlsl. Its headers are intentionally retained for now; the
// define-flattening plan moves this ABI into mydonut-owned headers.
void buffer_loads(
    in uint i_vertex : SV_VertexID,
    in uint i_instance : SV_InstanceID,
    out float4 o_position : SV_Position,
    out float2 o_texCoord : TEXCOORD)
{
    const uint compactedSlot = i_instance + g_Push.startInstanceLocation;
    const uint drawInstanceID = t_DrawInstanceIDs[compactedSlot];
    const DrawInstanceData drawInstance = t_DrawInstances[drawInstanceID];
    i_vertex += g_Push.startVertexLocation;

#ifdef TARGET_D3D11
    const InstanceData instance = LoadInstanceData(
        t_Instances,
        drawInstance.instanceID * c_SizeOfInstanceData);
#else
    const InstanceData instance = t_Instances[drawInstance.instanceID];
#endif

    float3 pos = asfloat(t_Vertices.Load3(
        g_Push.positionOffset + i_vertex * c_SizeOfPosition));
    float2 texCoord = asfloat(t_Vertices.Load2(
        g_Push.texCoordOffset + i_vertex * c_SizeOfTexcoord));

    float3 worldPos = mul(instance.transform, float4(pos, 1.0));
    o_texCoord = texCoord;
    o_position = mul(float4(worldPos, 1.0), g_Depth.matWorldToClip);
}

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

#include "..\\common\\mydonut_scene_abi.hlsli"

cbuffer c_Depth : register(b2, space2)
{
    DepthPassConstants g_Depth;
};

StructuredBuffer<InstanceData> t_Instances : register(t10, space1);
ByteAddressBuffer t_Vertices : register(t11, space1);
StructuredBuffer<DrawInstanceData> t_DrawInstances : register(t12, space1);
StructuredBuffer<uint> t_DrawInstanceIDs : register(t13, space1);

cbuffer c_Push : register(b1, space1)
{
    DepthPushConstants g_Push;
};
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

    const InstanceData instance = t_Instances[drawInstance.instanceID];

    float3 pos = asfloat(t_Vertices.Load3(
        g_Push.positionOffset + i_vertex * SizeOfPosition));
    float2 texCoord = asfloat(t_Vertices.Load2(
        g_Push.texCoordOffset + i_vertex * SizeOfTexcoord));

    float3 worldPos = mul(instance.transform, float4(pos, 1.0));
    o_texCoord = texCoord;
    o_position = mul(float4(worldPos, 1.0), g_Depth.matWorldToClip);
}

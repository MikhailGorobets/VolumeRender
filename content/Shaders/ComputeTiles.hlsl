/*
 * MIT License
 *
 * Copyright(c) 2021-2023 Mikhail Gorobets
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this softwareand associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions :
 *
 * The above copyright noticeand this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Common.hlsl"

#define WAVEFRONT_SIZE (THREAD_GROUP_SIZE_X) * (THREAD_GROUP_SIZE_Y)

Texture2D<float3> TextureColorSRV : register(t0);
Texture2D<float> TextureDepthSRV : register(t1);
AppendStructuredBuffer<uint> BufferTiles : register(u0);

groupshared float SharedBuffer[WAVEFRONT_SIZE];

float Luminance(float3 color)
{
    return dot(float3(0.2126, 0.7152, 0.0722), color);
}

float ReductionSum(uint lineID)
{
    [unrool(WAVEFRONT_SIZE / 2)]
    for (uint stride = WAVEFRONT_SIZE / 2; stride > 0; stride = stride >> 1)
    {
        if (lineID < stride) 
            SharedBuffer[lineID] += SharedBuffer[lineID + stride];
        GroupMemoryBarrierWithGroupSync();
    }
    return SharedBuffer[0];
}

float ComputeSum(uint3 thredID, uint lineID, Texture2D<float3> sourceTexture)
{
    SharedBuffer[lineID] = Luminance(sourceTexture[thredID.xy]);
    GroupMemoryBarrierWithGroupSync();
    return ReductionSum(lineID);
}

float ComputeSum(uint3 thredID, uint lineID, Texture2D<float> sourceTexture)
{
    SharedBuffer[lineID] = sourceTexture[thredID.xy];
    GroupMemoryBarrierWithGroupSync();
    return ReductionSum(lineID);
}

[numthreads(1, 1, 1)]
void ResetTiles(uint3 groupID : SV_GroupID, uint lineID : SV_GroupIndex)
{
    BufferTiles.Append((0xFFFF & groupID.x) | ((0xFFFF & groupID.y) << 16));
}


[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void ComputeTiles(uint3 thredID : SV_DispatchThreadID, uint lineID : SV_GroupIndex, uint3 groupID : SV_GroupID)
{
    const uint tileID = (0xFFFF & groupID.x) | ((0xFFFF & groupID.y) << 16);
       
    float colorSum = 0.75 * ComputeSum(thredID, lineID, TextureColorSRV);
    float depthSum = 0.25 * ComputeSum(thredID, lineID, TextureDepthSRV);
    float totalSum = colorSum + depthSum;
     
    if (totalSum > 0 && lineID == 0)
        BufferTiles.Append(tileID);
}

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

Texture2D<float3> TextureColorSRV : register(t0);
StructuredBuffer<uint> BufferDispersionTiles : register(t1);
RWTexture2D<float4> TextureColorSumUAV : register(u0);

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void Accumulate(uint3 threadID : SV_GroupThreadID, uint3 groupID : SV_GroupID)
{
    uint2 id = GetThreadIDFromTileList(BufferDispersionTiles, groupID.x, threadID.xy);
    float alpha = 1.0f / (FrameBuffer.FrameIndex + 1.0f);
    TextureColorSumUAV[id] = lerp(TextureColorSumUAV[id], float4(TextureColorSRV[id].xyz, 1.0), float4(alpha, alpha, alpha, alpha));
}

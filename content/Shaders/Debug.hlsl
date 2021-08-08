/*
 * MIT License
 *
 * Copyright(c) 2021 Mikhail Gorobets
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

StructuredBuffer<uint> BufferDispersionTiles: register(t0);

struct InputOutput {
    float4 Position: SV_Position;
};

float2 ScreenSpaceToNDC(float2 pixel) {
    float2 ncdXY = 2.0f * (pixel.xy) / FrameBuffer.RenderTargetDim - 1.0f;
    ncdXY.y *= -1.0f;
    return ncdXY;
}

void DebugTilesVS(uint id: SV_VertexID, out InputOutput output) {
    uint2 pixel = GetThreadIDFromTileList(BufferDispersionTiles, id, uint2(THREAD_GROUP_SIZE_X / 2, THREAD_GROUP_SIZE_Y / 2));
    output.Position = float4(ScreenSpaceToNDC(float2(pixel.x, pixel.y)), 0.0, 1.0);
}

InputOutput GenerateOutputGS(float2 screenCoord) {
    InputOutput result;
    result.Position = float4(screenCoord, 0.0, 1.0);
    return result;
}


[maxvertexcount(5)]
void DebugTilesGS(point InputOutput input[1], inout LineStream<InputOutput> stream) {
    
    float offsetX = (THREAD_GROUP_SIZE_X) * FrameBuffer.InvRenderTargetDim.x;
    float offsetY = (THREAD_GROUP_SIZE_Y) * FrameBuffer.InvRenderTargetDim.y;
    
    float2 x = input[0].Position.xy + float2(-offsetX, +offsetY) + float2(-FrameBuffer.InvRenderTargetDim.x, +FrameBuffer.InvRenderTargetDim.y);
    float2 y = input[0].Position.xy + float2(+offsetX, +offsetY);
    float2 z = input[0].Position.xy + float2(+offsetX, -offsetY);
    float2 w = input[0].Position.xy + float2(-offsetX, -offsetY);
    
    stream.Append(GenerateOutputGS(x));
    stream.Append(GenerateOutputGS(y));
    stream.Append(GenerateOutputGS(z));
    stream.Append(GenerateOutputGS(w));
    stream.Append(GenerateOutputGS(x));
    stream.RestartStrip();
}

float4 DebugTilesPS(InputOutput input): SV_TARGET0 {
    return float4(0.0, 0.0, 1.0, 1.0f);
}
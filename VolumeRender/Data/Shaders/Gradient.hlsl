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

SamplerState        SamplerPoint: register(s0);
Texture3D<float>    TextureSrc: register(t0);
RWTexture3D<float4> TextureDst: register(u0);


float3 Gradient(uint3 position) {
    float3 dimension;
    TextureSrc.GetDimensions(dimension.x, dimension.y, dimension.z);
    float3 texcoord = (position + float3(0.5, 0.5, 0.5)) / dimension;
    return ComputeGradientFiltered(TextureSrc, SamplerPoint, texcoord, float3(1.0 / dimension.x, 1.0 / dimension.y, 1.0 / dimension.z));
}

[numthreads(4, 4, 4)]
void ComputeGradient(uint3 thredID: SV_DispatchThreadID, uint lineID: SV_GroupIndex) {  
    float3 gradient = Gradient(thredID);
    TextureDst[thredID] = float4(normalize(gradient), length(gradient));
}
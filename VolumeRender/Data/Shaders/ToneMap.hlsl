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

Texture2D<float4>   TextureHDR : register(t0);
RWTexture2D<float4> TextureLDR : register(u0);

float3 Uncharted2Function(float A, float B, float C, float D, float E, float F, float3 x) {
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 ToneMapUncharted2Function(float3 x, float exposure) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;
    const float W = 11.2;

    float3 numerator   = Uncharted2Function(A, B, C, D, E, F, x) * exposure;
    float3 denominator = Uncharted2Function(A, B, C, D, E, F, W);

    return numerator / denominator;

}

float3 LinearToSRGB(float3 color) {
    float3 sRGBLo = color * 12.92;
    const float powExp = 1.0 / 2.2f;
    float3 sRGBHi = (pow(abs(color), float3(powExp, powExp, powExp)) * 1.055) - 0.055;
    float3 sRGB;
    sRGB.x = (color.x <= 0.0031308) ? sRGBLo.x : sRGBHi.x;
    sRGB.y = (color.y <= 0.0031308) ? sRGBLo.y : sRGBHi.y;
    sRGB.z = (color.z <= 0.0031308) ? sRGBLo.z : sRGBHi.z;
    return sRGB;
}

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void ToneMap(uint3 id : SV_DispatchThreadID) {
    float3 colorHDR = TextureHDR.Load(int3(id.xy, 0)).xyz;
    TextureLDR[id.xy] = float4(ToneMapUncharted2Function(colorHDR, FrameBuffer.Exposure), 1.0f);
}
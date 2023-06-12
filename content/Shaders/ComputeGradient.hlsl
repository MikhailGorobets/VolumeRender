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

Texture3D<float> TextureSrc : register(t0);
Texture1D<float> TextureTransferFunction : register(t1);
RWTexture3D<float4> TextureDst : register(u0);

float GetIntensity(int3 location, int3 offset)
{
    int3 dimension;
    TextureSrc.GetDimensions(dimension.x, dimension.y, dimension.z);
    return TextureSrc.Load(uint4(clamp(location + offset, int3(0, 0, 0), int3(dimension.x, dimension.y, dimension.z) - int3(1, 1, 1)), 0));
}

float3 ComputeGradientCD(int3 location)
{
    float dx = GetIntensity(location, int3(1, 0, 0)) - GetIntensity(location, int3(-1, 0, 0));
    float dy = GetIntensity(location, int3(0, 1, 0)) - GetIntensity(location, int3(0, -1, 0));
    float dz = GetIntensity(location, int3(0, 0, 1)) - GetIntensity(location, int3(0, 0, -1));
    return float3(dx, dy, dz);
}

float3 ComputeGradientFD(int3 location)
{
    float p = GetIntensity(location, int3(0, 0, 0));
    float dx = GetIntensity(location, int3(1, 0, 0)) - p;
    float dy = GetIntensity(location, int3(0, 1, 0)) - p;
    float dz = GetIntensity(location, int3(0, 0, 1)) - p;
    return float3(dx, dy, dz);
}

float3 ComputeGradientFiltered(int3 location)
{
    float3 G0 = ComputeGradientCD(location + int3(0, 0, 0));
    float3 G1 = ComputeGradientCD(location + int3(0, 0, 1));
    float3 G2 = ComputeGradientCD(location + int3(0, 1, 0));
    float3 G3 = ComputeGradientCD(location + int3(0, 1, 1));
    float3 G4 = ComputeGradientCD(location + int3(1, 0, 0));
    float3 G5 = ComputeGradientCD(location + int3(1, 0, 1));
    float3 G6 = ComputeGradientCD(location + int3(1, 1, 0));
    float3 G7 = ComputeGradientCD(location + int3(1, 1, 1));
 
    float3 L0 = lerp(lerp(G0, G2, 0.5), lerp(G4, G6, 0.5), 0.5);
    float3 L1 = lerp(lerp(G1, G3, 0.5), lerp(G5, G7, 0.5), 0.5);
    return lerp(G0, lerp(L0, L1, 0.5), 0.5);
}

float3 ComputeGradientSobel(uint3 location)
{
    int Gx[3][3][3] =
    {
        { { -1, -2, -1 }, { -2, -4, -2 }, { -1, -2, -1 } },
        { { +0, +0, +0 }, { +0, +0, +0 }, { +0, +0, +0 } },
        { { +1, +2, +1 }, { +2, +4, +2 }, { +1, +2, +1 } }
    };
    
    int Gy[3][3][3] =
    {
        { { -1, -2, -1 }, { +0, +0, +0 }, { +1, +2, +1 } },
        { { -2, -4, -2 }, { +0, +0, +0 }, { +2, +4, +2 } },
        { { -1, -2, -1 }, { +0, +0, +0 }, { +1, +2, +1 } }
    };

    int Gz[3][3][3] =
    {
        { { -1, +0, +1 }, { -2, +0, +2 }, { -1, 0, +1 } },
        { { -2, +0, +2 }, { -4, +0, +4 }, { -2, 0, +2 } },
        { { -1, +0, +1 }, { -1, +0, +1 }, { -1, 0, +1 } }
    };
    
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            for (int z = -1; z <= 1; z++)
            {
                float intensity = GetIntensity(location, int3(x, y, z));
                dx += Gx[x + 1][y + 1][z + 1] * intensity;
                dy += Gy[x + 1][y + 1][z + 1] * intensity;
                dz += Gz[x + 1][y + 1][z + 1] * intensity;
            }
        }
    }
    return float3(dx, dy, dz) / 16.0;
}

float3 Gradient(uint3 location)
{
    return ComputeGradientSobel(location);
}

[numthreads(4, 4, 4)]
void ComputeGradient(uint3 thredID : SV_DispatchThreadID, uint lineID : SV_GroupIndex)
{
    float3 gradient = Gradient(thredID);
    TextureDst[thredID] = float4(gradient, 0); //float4(length(gradient) < FLT_MIN ? float3(0.0f, 0.0f, 0.0f) : normalize(gradient), length(gradient));
}

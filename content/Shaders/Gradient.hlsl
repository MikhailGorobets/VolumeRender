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

SamplerState SamplerPoint:  register(s0);
SamplerState SamplerLinear: register(s1);
Texture3D<float>    TextureSrc: register(t0);
Texture1D<float>    TextureTransferFunction:  register(t1);
RWTexture3D<float4> TextureDst: register(u0);


float GetIntensity(float3 texcoord) {
    return TextureSrc.SampleLevel(SamplerPoint, texcoord, 0);
}

float GetOpacity(float3 texcoord) { 
    return TextureTransferFunction.SampleLevel(SamplerLinear, GetIntensity(texcoord), 0);
}

float3 ComputeGradientCD(float3 texcoord, float3 dimension) {
    float3 dt = float3(1.0 / dimension.x, 1.0 / dimension.y, 1.0 / dimension.z);  
    float dx = GetIntensity(texcoord + float3(dt.x, 0.0f, 0.0f)) - GetIntensity(texcoord - float3(dt.x, 0.0f, 0.0f));
    float dy = GetIntensity(texcoord + float3(0.0f, dt.y, 0.0f)) - GetIntensity(texcoord - float3(0.0f, dt.y, 0.0f));
    float dz = GetIntensity(texcoord + float3(0.0f, 0.0f, dt.z)) - GetIntensity(texcoord - float3(0.0f, 0.0f, dt.z));
    return float3(dx, dy, dz);
}

float3 ComputeGradientFD(float3 texcoord, float3 dimension) {
    float3 dt = float3(1.0 / dimension.x, 1.0 / dimension.y, 1.0 / dimension.z);   
    float p  = GetIntensity(texcoord);
    float dx = GetIntensity(texcoord + float3(dt.x, 0.0f, 0.0f)) - p;
    float dy = GetIntensity(texcoord + float3(0.0f, dt.y, 0.0f)) - p;
    float dz = GetIntensity(texcoord + float3(0.0f, 0.0f, dt.z)) - p;
    return float3(dx, dy, dz);
}

float3 ComputeGradientFiltered(float3 texcoord, float3 dimension) {
    float3 dt = float3(1.0 / dimension.x, 1.0 / dimension.y, 1.0 / dimension.z);
    
    float3 G0 = ComputeGradientCD(texcoord, dimension);
    float3 G1 = ComputeGradientCD(texcoord + float3(-dt.x, -dt.y, -dt.z), dimension);
    float3 G2 = ComputeGradientCD(texcoord + float3(+dt.x, +dt.y, +dt.z), dimension);
    float3 G3 = ComputeGradientCD(texcoord + float3(-dt.x, +dt.y, -dt.z), dimension);
    float3 G4 = ComputeGradientCD(texcoord + float3(+dt.x, -dt.y, +dt.z), dimension);
    float3 G5 = ComputeGradientCD(texcoord + float3(-dt.x, -dt.y, +dt.z), dimension);
    float3 G6 = ComputeGradientCD(texcoord + float3(+dt.x, +dt.y, -dt.z), dimension);
    float3 G7 = ComputeGradientCD(texcoord + float3(-dt.x, +dt.y, +dt.z), dimension);
    float3 G8 = ComputeGradientCD(texcoord + float3(+dt.x, -dt.y, -dt.z), dimension);
 
    float3 L0 = lerp(lerp(G1, G2, 0.5), lerp(G3, G4, 0.5), 0.5);
    float3 L1 = lerp(lerp(G5, G6, 0.5), lerp(G7, G8, 0.5), 0.5);
    return lerp(G0, lerp(L0, L1, 0.5), 0.75);
}


float3 ComputeGradientSobel(float3 texcoord, float3 dimension) {   
    int kx[3][3][3] ={
        { { -1, -2, -1 }, { -2, -4, -2 }, { -1, -2, -1 } },
        { { +0, +0, +0 }, { +0, +0, +0 }, { +0, +0, +0 } },
        { { +1, +2, +1 }, { +2, +4, +2 }, { +1, +2, +1 } }
    };
    
    int ky[3][3][3] = {
        { { -1, -2, -1 }, { +0, +0, +0 }, { +1, +2, +1 } },
        { { -2, -4, -2 }, { +0, +0, +0 }, { +2, +4, +2 } },
        { { -1, -2, -1 }, { +0, +0, +0 }, { +1, +2, +1 } }
    };

    int kz[3][3][3] = {
        { { -1, +0, +1 }, { -2, +0, +2 }, { -1, 0, +1 } },
        { { -2, +0, +2 }, { -4, +0, +4 }, { -2, 0, +2 } },
        { { -1, +0, +1 }, { -1, +0, +1 }, { -1, 0, +1 } }
    };
    
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                float intensity = GetIntensity(texcoord + float3(x, y, z) / dimension);
                dx += kx[x + 1][y + 1][z + 1] * intensity;
                dy += ky[x + 1][y + 1][z + 1] * intensity;
                dz += kz[x + 1][y + 1][z + 1] * intensity;
            }
        }    
    }
    return float3(dx, dy, dz);
}


float3 Gradient(uint3 position) {
    float3 dimension;
    TextureSrc.GetDimensions(dimension.x, dimension.y, dimension.z);
    float3 texcoord = (position + 0.5) / dimension;
    return ComputeGradientSobel(texcoord, dimension);
}


[numthreads(4, 4, 4)]
void ComputeGradient(uint3 thredID: SV_DispatchThreadID, uint lineID: SV_GroupIndex) {  
    float3 gradient = Gradient(thredID);
    TextureDst[thredID] = float4(length(gradient) < FLT_MIN ? float3(0.0f, 0.0f, 0.0f) : normalize(gradient), length(gradient));
}
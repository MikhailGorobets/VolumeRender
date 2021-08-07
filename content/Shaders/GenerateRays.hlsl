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

struct VolumeDesc {
    AABB  BoundingBox;
    float StepSize;
    float DensityScale;
};

struct ScatterEvent {
    float3 Position;
    float3 Normal;
    float3 Diffuse;
    float3 Specular;
    float  Roughness;
    bool   IsValid;
};

Texture3D<float>  TextureVolumeIntensity: register(t0);
Texture3D<float4> TextureVolumeGradient: register(t1);
Texture1D<float3> TextureTransferFunctionDiffuse: register(t2);
Texture1D<float3> TextureTransferFunctionSpecular: register(t3);
Texture1D<float1> TextureTransferFunctionRoughness: register(t4);
Texture1D<float1> TextureTransferFunctionOpacity: register(t5);
StructuredBuffer<uint> BufferDispersionTiles: register(t6);

RWTexture2D<float3> TextureDiffuseUAV: register(u0);
RWTexture2D<float3> TextureSpecularUAV: register(u1);
RWTexture2D<float4> TextureNormalUAV: register(u2);
RWTexture2D<float>  TextureDepthUAV: register(u3);

SamplerState SamplerPoint: register(s0);
SamplerState SamplerLinear: register(s1);
SamplerState SamplerAnisotropic: register(s2);


float GetIntensity(VolumeDesc desc, float3 position) {
    return TextureVolumeIntensity.SampleLevel(SamplerLinear, GetNormalizedTexcoord(position, desc.BoundingBox), 0);
}

float4 GetGradient(VolumeDesc desc, float3 position) {
    return TextureVolumeGradient.SampleLevel(SamplerLinear, GetNormalizedTexcoord(position, desc.BoundingBox), 0);
}
 
float GetOpacity(VolumeDesc desc, float3 position) {
    return TextureTransferFunctionOpacity.SampleLevel(SamplerLinear, GetIntensity(desc, position), 0);
}

float3 GetDiffuse(VolumeDesc desc, float3 position) {
    return TextureTransferFunctionDiffuse.SampleLevel(SamplerLinear, GetIntensity(desc, position), 0);
}

float3 GetSpecular(VolumeDesc desc, float3 position) {
    return TextureTransferFunctionSpecular.SampleLevel(SamplerLinear, GetIntensity(desc, position), 0);
}

float GetRoughness(VolumeDesc desc, float3 position) {
    return TextureTransferFunctionRoughness.SampleLevel(SamplerLinear, GetIntensity(desc, position), 0);
}


ScatterEvent RayMarching(Ray ray, VolumeDesc desc, inout CRNG rng) {
    ScatterEvent event;
    event.Position = float3(0.0f, 0.0f, 0.0f);
    event.Normal = float3(0.0f, 0.0f, 0.0f);
    event.Diffuse = float3(0.0f, 0.0f, 0.0f);
    event.Specular = float3(0.0f, 0.0f, 0.0f);
    event.Roughness = 0.0f;
    event.IsValid = false;
      
    Intersection intersect = IntersectAABB(ray, desc.BoundingBox);
	
    [branch]
    if (intersect.Max < intersect.Min)
        return event;

    const float minT = max(intersect.Min, ray.Min);
    const float maxT = min(intersect.Max, ray.Max);
    
    const float threshold = -log(Rand(rng)) / desc.DensityScale;
	
    float sum = 0.0f;
    float t = minT + Rand(rng) * desc.StepSize;
    float3 position = float3(0.0, 0.0, 0.0f);
    
    [loop]
    while (sum < threshold) {
        position = ray.Origin + t * ray.Direction;
        [branch]
        if (t >= maxT)
            return event;

        sum += desc.DensityScale * GetOpacity(desc, position) * desc.StepSize;
        t += desc.StepSize;
    }
   
    const float4 gradient = GetGradient(desc, position);
	
    [branch]
    if (gradient.a < FLT_EPSILON) 
        return event;
    
    const float3 diffuse = GetDiffuse(desc, position);
    const float3 specular = GetSpecular(desc, position);
    const float roughness = GetRoughness(desc, position);
    
    event.IsValid = true;
    event.Normal = -normalize(gradient.xyz);
    event.Normal = dot(event.Normal, -ray.Direction) < 0.0f ? -event.Normal : event.Normal;
    event.Position = position + 0.001 * event.Normal; 
    event.Diffuse = diffuse;
    event.Specular = specular;
    event.Roughness = roughness;
    return event;
}

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void GenerateRays(uint3 thredID: SV_GroupThreadID, uint3 groupID: SV_GroupID) {
    uint2 id = GetThreadIDFromTileList(BufferDispersionTiles, groupID.x, thredID.xy);
    
    CRNG rng = InitCRND(id, FrameBuffer.FrameIndex);
    Ray ray = CreateCameraRay(id, FrameBuffer.FrameOffset, FrameBuffer.RenderTargetDim);
 	
    VolumeDesc desc;
    desc.BoundingBox.Min = FrameBuffer.BoundingBoxMin;
    desc.BoundingBox.Max = FrameBuffer.BoundingBoxMax;
    desc.StepSize = FrameBuffer.StepSize;
    desc.DensityScale = FrameBuffer.Density;
       
    ScatterEvent event = RayMarching(ray, desc, rng);
    if (event.IsValid) {
        float3 normal = { 0.0f, 0.0f, 0.0f };
        float4 position = { 0.0f, 0.0f, 0.0f, 0.0f };
        
        normal = event.Normal; 
        position = mul(FrameBuffer.WorldViewProjectionMatrix, float4(event.Position, 1.0));
        position /= position.w;
   
        TextureDiffuseUAV[id] = event.Diffuse;
        TextureSpecularUAV[id] = event.Specular;
        TextureNormalUAV[id] = float4(normal, event.Roughness);
        TextureDepthUAV[id] = position.z;
    }
}

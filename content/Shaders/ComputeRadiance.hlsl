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

struct VolumeDesc
{
    AABB BoundingBox;
    float StepSize;
    float DensityScale;
};

struct GBuffer
{
    float3 Position;
    float3 Normal;
    float3 View;
    float3 Diffuse;
    float3 Specular;
    float Roughness;
};

Texture3D<float> TextureVolumeIntensity : register(t0);
Texture1D<float1> TextureTransferFunctionOpacity : register(t1);
Texture2D<float3> TextureDiffuse : register(t2);
Texture2D<float3> TextureSpecular : register(t3);
Texture2D<float4> TextureNormal : register(t4);
Texture2D<float> TextureDepth : register(t5);
Texture2D<float3> TextureEnvironment : register(t6);
StructuredBuffer<uint> BufferDispersionTiles : register(t7);

RWTexture2D<float3> TextureRadianceAV : register(u0);

SamplerState SamplerPoint : register(s0);
SamplerState SamplerLinear : register(s1);
SamplerState SamplerAnisotropic : register(s2);

float3x3 GetTangentSpace(float3 normal)
{
    const float3 helper = abs(normal.y) > 0.999 ? float3(0, 0, 1) : float3(0, 1, 0);
    const float3 tangent = normalize(cross(normal, helper));
    const float3 binormal = cross(normal, tangent);
    return float3x3(tangent, binormal, normal);
}

float3 FresnelSchlick(float3 F0, float VdotH)
{
    return F0 + (1.0f - F0) * pow(1.0 - VdotH, 5.0);
}

float GGX_PartialGeometry(float NdotX, float alpha)
{
    const float aa = alpha * alpha;
    return 2.0 * NdotX * rcp(max((NdotX + sqrt(aa + (1.0 - aa) * (NdotX * NdotX))), 1e-6));
}

float GGX_Distribution(float NdotH, float alpha)
{
    const float aa = alpha * alpha;
    const float f = (aa - 1.0) * NdotH * NdotH + 1.0;
    return aa * rcp(M_PI * f * f);
}

float3 SampleGGXDir(float3 normal, float alpha, inout CRNG rng)
{
    float2 xi = float2(Rand(rng), Rand(rng));
    float phi = 2.0 * M_PI * xi.x;
    float cosTheta = sqrt(max(0.0f, (1.0 - xi.y) * rcp(1.0 + alpha * alpha * xi.y - xi.y)));
    float sinTheta = sqrt(max(0.0f, 1.0 - cosTheta * cosTheta));
    return mul(float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta), GetTangentSpace(normal));
}

float GetIntensity(VolumeDesc desc, float3 position)
{
    return TextureVolumeIntensity.SampleLevel(SamplerLinear, GetNormalizedTexcoord(position, desc.BoundingBox), 0);
}

float GetOpacity(VolumeDesc desc, float3 position)
{
    return TextureTransferFunctionOpacity.SampleLevel(SamplerLinear, GetIntensity(desc, position), 0);
}

float3 GetEnvironment(float3 direction)
{
    const float theta = acos(direction.y) / M_PI;
    const float phi = atan2(direction.x, -direction.z) / M_PI * 0.5f;
    return TextureEnvironment.SampleLevel(SamplerAnisotropic, float2(phi, theta), 0);
}

GBuffer LoadGBuffer(uint2 id, float2 offset, float2 invDimension, float4x4 invWVP)
{
    float2 ncdXY = ScreenSpaceToNDC(id, invDimension);
    float4 rayStart = mul(invWVP, float4(ncdXY, 0.0f, 1.0f));
    float4 rayEnd = mul(invWVP, float4(ncdXY, TextureDepth[id], 1.0f));
    rayStart /= rayStart.w;
    rayEnd /= rayEnd.w;
    
    float4 normal = TextureNormal[id];
    
    GBuffer buffer;
    buffer.Diffuse = TextureDiffuse[id];
    buffer.Specular = TextureSpecular[id];
    buffer.Roughness = normal.w;
    buffer.Normal = normal.xyz;
    buffer.View = normalize(rayStart.xyz - rayEnd.xyz);
    buffer.Position = rayEnd.xyz;
    return buffer;
}

bool RayMarching(Ray ray, VolumeDesc desc, inout CRNG rng)
{
    Intersection intersect = IntersectAABB(ray, desc.BoundingBox);
	
    [branch]
    if (intersect.Max < intersect.Min)
        return false;

    const float minT = max(intersect.Min, ray.Min);
    const float maxT = min(intersect.Max, ray.Max);
    
    const float threshold = -log(Rand(rng)) / desc.DensityScale;
	
    float sum = 0.0f;
    float t = minT + Rand(rng) * desc.StepSize;
    float3 position = float3(0.0, 0.0, 0.0f);
    
    [loop]
    while (sum < threshold)
    {
        position = ray.Origin + t * ray.Direction;
        [branch]
        if (t >= maxT)
            return false;
        sum += desc.DensityScale * GetOpacity(desc, position) * desc.StepSize;
        t += desc.StepSize;
    }
    return true;
}

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void ComputeRadiance(uint3 thredID : SV_GroupThreadID, uint3 groupID : SV_GroupID)
{
    uint2 id = GetThreadIDFromTileList(BufferDispersionTiles, groupID.x, thredID.xy);
    CRNG rng = InitCRND(id, FrameBuffer.FrameIndex);
    GBuffer buffer = LoadGBuffer(id, FrameBuffer.FrameOffset, FrameBuffer.InvRenderTargetDim, FrameBuffer.InvWorldViewProjectionMatrix);

    if (any(buffer.Diffuse))
    {
        VolumeDesc desc;
        desc.BoundingBox.Min = FrameBuffer.BoundingBoxMin;
        desc.BoundingBox.Max = FrameBuffer.BoundingBoxMax;
        desc.StepSize = FrameBuffer.StepSize;
        desc.DensityScale = FrameBuffer.Density;
      
        Ray ray;
        ray.Min = 0;
        ray.Max = FLT_MAX;
     
        float3 throughput = float3(0.0, 0.0, 0.0);
        
        const float3 N = buffer.Normal;
        const float3 V = buffer.View;
        const float alpha = buffer.Roughness * buffer.Roughness;
        
        const float3 H = SampleGGXDir(N, alpha, rng);
        const float3 F = FresnelSchlick(buffer.Specular, saturate(dot(V, H)));
					
        const float pd = length(1 - F);
        const float ps = length(F);
        const float pdf = ps / (ps + pd);
     	
        if (Rand(rng) < pdf)
        {
            const float3 L = reflect(-V, H);
            const float NdotL = saturate(dot(N, L));
            const float NdotV = saturate(dot(N, V));
            const float NdotH = saturate(dot(N, H));
            const float VdotH = saturate(dot(V, H));
		
            const float G = GGX_PartialGeometry(NdotV, alpha) * GGX_PartialGeometry(NdotL, alpha);
            ray.Origin = buffer.Position;
            ray.Direction = L;
            throughput += (G * F * VdotH) / (NdotV * NdotH + M_EPSILON) / (pdf);
        }
        else
        {
            const float3 L = SampleGGXDir(N, 1.0, rng);
            ray.Origin = buffer.Position;
            ray.Direction = L;
            throughput += (1 - F) * buffer.Diffuse / (1 - pdf);
        }
             
        bool isIntersect = RayMarching(ray, desc, rng);
        TextureRadianceAV[id] = !isIntersect * throughput * GetEnvironment(mul((float3x3) FrameBuffer.NormalMatrix, ray.Direction));;
    }
}

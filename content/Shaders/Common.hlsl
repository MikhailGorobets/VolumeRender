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

#pragma pack_matrix( row_major )

static const float M_PI = 3.14159265f;
static const float M_EPSILON = 1.0e-3f;

static const float FLT_EPSILON = 1.192092896e-07f;
static const float FLT_MAX = 3.402823466e+38f;
static const float FLT_MIN = 1.175494351e-38f;

cbuffer ConstantFrameBuffer : register(b0)
{
    struct
    {
        float4x4 ProjectionMatrix;
        float4x4 ViewMatrix;
        float4x4 WorldMatrix;
        float4x4 NormalMatrix;
    
        float4x4 InvProjectionMatrix;
        float4x4 InvViewMatrix;
        float4x4 InvWorldMatrix;
        float4x4 InvNormalMatrix;
    
        float4x4 ViewProjectionMatrix;
        float4x4 NormalViewMatrix;
        float4x4 WorldViewProjectionMatrix;

        float4x4 InvViewProjectionMatrix;
        float4x4 InvNormalViewMatrix;
        float4x4 InvWorldViewProjectionMatrix;

        uint FrameIndex;
        float StepSize;
        float2 FrameOffset;
        
        float2 InvRenderTargetDim;
        float2 RenderTargetDim;
        
        float Density;
        float3 BoundingBoxMin;

        float Exposure;
        float3 BoundingBoxMax;
    } FrameBuffer;
}

struct Ray
{
    float3 Origin;
    float3 Direction;
    float Min;
    float Max;
};

struct Intersection
{
    float Min;
    float Max;
};

struct AABB
{
    float3 Min;
    float3 Max;
};

struct CRNG
{
    uint Seed;
};

float Max3(float a, float b, float c)
{
    return max(max(a, b), c);
}

float Min3(float a, float b, float c)
{
    return min(min(a, b), c);
}

float Med3(float a, float b, float c)
{
    return clamp(a, min(b, c), max(b, c));
}

float2 ScreenSpaceToNDC(float2 pixel, float2 invDimension)
{
    float2 ndc = 2.0f * (pixel.xy + 0.5) * invDimension - 1.0f;
    return float2(ndc.x, -ndc.y);
}

Intersection IntersectAABB(Ray ray, AABB aabb)
{
    Intersection intersect = { +FLT_MAX, -FLT_MAX };

    const float3 invR = rcp(ray.Direction);
    const float3 bot = invR * (aabb.Min - ray.Origin);
    const float3 top = invR * (aabb.Max - ray.Origin);
    const float3 tmin = min(top, bot);
    const float3 tmax = max(top, bot);
   
    const float largestMin = Max3(tmin.x, tmin.y, tmin.z);
    const float largestMax = Min3(tmax.x, tmax.y, tmax.z);
    
    intersect.Min = largestMin;
    intersect.Max = largestMax;
    return intersect;
}

Ray CreateCameraRay(uint2 id, float2 offset, float2 invDimension, float4x4 invWVP)
{
    float2 ncdXY = ScreenSpaceToNDC(id, invDimension);

    float4 rayStart = mul(invWVP, float4(ncdXY, 0.0, 1.0f));
    float4 rayEnd = mul(invWVP, float4(ncdXY, 1.0f, 1.0f));

    rayStart.xyz /= rayStart.w;
    rayEnd.xyz /= rayEnd.w;

    Ray ray;
    ray.Direction = normalize(rayEnd.xyz - rayStart.xyz);
    ray.Origin = rayStart.xyz;
    ray.Min = 0;
    ray.Max = distance(rayEnd.xyz, rayStart.xyz);;

    return ray;
}

uint PCGHash(uint seed)
{
    uint state = seed * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float Rand(inout CRNG rng)
{
    rng.Seed = PCGHash(rng.Seed);
    uint u = 0x3f800000 | (rng.Seed >> 9);
    return asfloat(u) - 1.0;
}

CRNG InitCRND(uint2 id, uint frameIndex)
{
    CRNG rng = { frameIndex + PCGHash((id.x << 16) | id.y) };
    return rng;
}

float3 GetNormalizedTexcoord(float3 position, AABB aabb)
{
    return (position - aabb.Min) / (aabb.Max - aabb.Min);
}

float3 GetWorldPosition(float3 texcoord, AABB aabb)
{
    return texcoord * (aabb.Max - aabb.Min) + aabb.Min;
}

uint2 GetThreadIDFromTileList(StructuredBuffer<uint> tiles, uint threadGroupID, uint2 offset)
{
    uint packedTile = tiles[threadGroupID];
    uint2 unpackedGroupID = uint2(packedTile & 0xFFFF, (packedTile >> 16) & 0xFFFF);
    return uint2(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y) * unpackedGroupID + offset;
}

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

#pragma pack_matrix( row_major )

static const float M_PI = 3.14159265f;
static const float M_EPSILON = 1.0e-3f;

static const float FLT_EPSILON = 1.192092896e-07f;
static const float FLT_MAX = 3.402823466e+38f;
static const float FLT_MIN = 1.175494351e-38f;

cbuffer ConstantFrameBuffer: register(b0) {
    struct {
        float4x4 ViewProjectionMatrix;
        float4x4 NormalViewMatrix;
        float4x4 WorldViewProjectionMatrix;
        float4x4 ViewMatrix;
        float4x4 WorldMatrix;
        float4x4 NormalMatrix;

        float4x4 InvViewProjection;
        float4x4 InvNormalViewMatrix;
        float4x4 InvWorldViewProjectionMatrix;
        float4x4 InvViewMatrix;
        float4x4 InvWorldMatrix;
        float4x4 InvNormalMatrix;

        float Dispersion;
        uint FrameIndex;
        uint TraceDepth;
        float StepSize;

        float  Density;
        float3 BoundingBoxMin;

        float  Exposure;
        float3 BoundingBoxMax;

        float2 FrameOffset;
        float2 RenderTargetDim;
    } FrameBuffer;
}

struct Ray {
    float3 Origin;
    float3 Direction;
    float Min;
    float Max;
};

struct Intersection {
    float Min;
    float Max;
};

//struct ScatterEvent {
//    float3 Position;
//    float3 Normal;
//    float3 Diffuse;
//    float3 Specular;
//    float Roughness;
//    float Alpha;
//    float Opacity;
//    bool IsValid;
//};

struct AABB {
    float3 Min;
    float3 Max;
};

struct CRNG {
    uint2 Seed;
};

Intersection IntersectAABB(Ray ray, AABB aabb) {
    Intersection intersect;
    intersect.Min = 0.0f;
    intersect.Max = FLT_MAX;

    const float3 invR = rcp(ray.Direction);
    const float3 bot = invR * (aabb.Min - ray.Origin);
    const float3 top = invR * (aabb.Max - ray.Origin);
    const float3 tmin = min(top, bot);
    const float3 tmax = max(top, bot);

    const float largestMin = max(max(tmin.x, tmin.y), tmin.z);
    const float largestMax = min(min(tmax.x, tmax.y), tmax.z);

    intersect.Min = largestMin; //> 0.0f ? largestMin : 0.0f;
    intersect.Max = largestMax;

    return intersect;
}

Ray CreateCameraRay(uint2 id, float2 offset, float2 dimension) {
    float2 ncdXY = 2.0f * (id.xy + offset) / dimension - 1.0f;
    ncdXY.y *= -1.0f;
   
    float4 rayStart = mul(FrameBuffer.InvWorldViewProjectionMatrix, float4(ncdXY, -1.0f, 1.0f));
    float4 rayEnd = mul(FrameBuffer.InvWorldViewProjectionMatrix, float4(ncdXY, 1.0f, 1.0f));

    rayStart.xyz /= rayStart.w;
    rayEnd.xyz /= rayEnd.w;

    Ray ray;
    ray.Direction = normalize(rayEnd.xyz - rayStart.xyz);
    ray.Origin = rayStart;
    ray.Min = 0;
    ray.Max = length(rayEnd.xyz - rayStart.xyz);;

    return ray;
}

uint Hash(uint seed) {
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

uint RngNext(inout CRNG rng) {
    uint result = rng.Seed.x * 0x9e3779bb;

    rng.Seed.y ^= rng.Seed.x;
    rng.Seed.x = ((rng.Seed.x << 26) | (rng.Seed.x >> (32 - 26))) ^ rng.Seed.y ^ (rng.Seed.y << 9);
    rng.Seed.y =  (rng.Seed.x << 13) | (rng.Seed.x >> (32 - 13));

    return result;
}

float Rand(inout CRNG rng) {
    uint u = 0x3f800000 | (RngNext(rng) >> 9);
    return asfloat(u) - 1.0;
}

CRNG InitCRND(uint2 id, uint frameIndex) {
    uint s0 = (id.x << 16) | id.y;
    uint s1 = frameIndex;

    CRNG rng;
    rng.Seed.x = Hash(s0);
    rng.Seed.y = Hash(s1);
    RngNext(rng);
    return rng;
}


float3 GetNormalizedTexcoord(float3 position, AABB aabb) {
    return (position - aabb.Min) / (aabb.Max - aabb.Min);
}

float3 GetWorldPosition(float3 texcoord, AABB aabb) {
    return texcoord * (aabb.Max - aabb.Min) + aabb.Min;
}

uint2 GetThreadIDFromTileList(StructuredBuffer<uint> tiles, uint threadGroupID, uint2 offset) {
    uint packedTile = tiles[threadGroupID];
    uint2 unpackedGroupID = uint2(packedTile & 0xFFFF, (packedTile >> 16) & 0xFFFF);
    return uint2(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y) * unpackedGroupID + offset;
}

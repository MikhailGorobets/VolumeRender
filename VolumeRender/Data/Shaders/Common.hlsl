
#pragma once

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
  
        float4x4 ViewProjectionMatrix;
        float4x4 WorldMatrix;
        float4x4 NormalMatrix;

        float4x4 InvViewProjection;
        float4x4 InvWorldMatrix;
        float4x4 InvNormalMatrix;

        float DenoiserStrange;
        uint  FrameIndex;
        uint  TraceDepth;
        uint  StepCount;

        float  Density;
        float3 BoundingBoxMin;

        float  Exposure;
        float3 BoundingBoxMax;

        float2 FrameOffset;
        float2 RenderTargetDim;


    } FrameBuffer;
}




struct Ray
{
    float3 Origin;
    float3 Direction;
    float  Min;
    float  Max;
};

struct Intersection {
    float Near;
    float Far;
    bool  IsValid;
};

struct ScatterEvent {
    float3 Position;
    float3 Normal;
    float3 View;
    float3 Diffuse;
    float3 Specular;
    float  Roughness;
    float  Alpha;
    float  Opacity;
    bool   IsValid;
};

struct AABB {
    float3 Min;
    float3 Max;
};

struct CRNG {
    uint2 Seed;
};



Intersection IntersectAABB(Ray ray, AABB aabb) {

    Intersection intersect;
    intersect.IsValid = false;
    intersect.Near = 0.0f;
    intersect.Far = FLT_MAX;

    const float3 invR = rcp(ray.Direction);
    const float3 bot = invR * (aabb.Min - ray.Origin);
    const float3 top = invR * (aabb.Max - ray.Origin);
    const float3 tmin = min(top, bot);
    const float3 tmax = max(top, bot);

    const float largestMin = max(max(tmin.x, tmin.y), tmin.z);
    const float largestMax = min(min(tmax.x, tmax.y), tmax.z);


    intersect.IsValid = largestMax > largestMin;
    intersect.Near = largestMin > 0.0f ? largestMin : 0.0f;
    intersect.Far = largestMax;

    return intersect;
}

Ray CreateCameraRay(uint2 id, float2 offset, float2 dimension) {
   
    float2 ncdXY = 2.0f * (id.xy + offset) / dimension - 1.0f;
    ncdXY.y *= -1.0f;

    float4 rayStart = mul(FrameBuffer.InvViewProjection, float4(ncdXY, 0.0f, 1.0f));
    float4 rayEnd = mul(FrameBuffer.InvViewProjection, float4(ncdXY, 1.0f, 1.0f));

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
    rng.Seed.y = (rng.Seed.x << 13) | (rng.Seed.x >> (32 - 13));

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

float3 ComputeGradientCD(Texture3D<float> volume, SamplerState samplerState, float3 texcoord, float3 gradientDelta) {
    float dx = volume.SampleLevel(samplerState, texcoord + float3(gradientDelta.x, 0.0f, 0.0f), 0) - volume.SampleLevel(samplerState, texcoord - float3(gradientDelta.x, 0.0f, 0.0f), 0);
    float dy = volume.SampleLevel(samplerState, texcoord + float3(0.0f, gradientDelta.y, 0.0f), 0) - volume.SampleLevel(samplerState, texcoord - float3(0.0f, gradientDelta.y, 0.0f), 0);
    float dz = volume.SampleLevel(samplerState, texcoord + float3(0.0f, 0.0f, gradientDelta.z), 0) - volume.SampleLevel(samplerState, texcoord - float3(0.0f, 0.0f, gradientDelta.z), 0);
    return float3(dx, dy, dz);
}

float3 ComputeGradientFD(Texture3D<float> volume, SamplerState samplerState, float3 texcoord, float3 gradientDelta) {
    float p = volume.SampleLevel(samplerState, texcoord, 0);
    float dx = volume.SampleLevel(samplerState, texcoord + float3(gradientDelta.x, 0.0f, 0.0f), 0) - p;
    float dy = volume.SampleLevel(samplerState, texcoord + float3(0.0f, gradientDelta.y, 0.0f), 0) - p;
    float dz = volume.SampleLevel(samplerState, texcoord + float3(0.0f, 0.0f, gradientDelta.z), 0) - p;
    return float3(dx, dy, dz);
}


float3 ComputeGradientFiltered(Texture3D<float> volume, SamplerState samplerState, float3 texcoord, float3 gradientDelta) {
	
    float3 G0 = ComputeGradientCD(volume, samplerState, texcoord, gradientDelta);
    float3 G1 = ComputeGradientCD(volume, samplerState, texcoord + float3(-gradientDelta.x, -gradientDelta.y, -gradientDelta.z), gradientDelta);
    float3 G2 = ComputeGradientCD(volume, samplerState, texcoord + float3(+gradientDelta.x, +gradientDelta.y, +gradientDelta.z), gradientDelta);
    float3 G3 = ComputeGradientCD(volume, samplerState, texcoord + float3(-gradientDelta.x, +gradientDelta.y, -gradientDelta.z), gradientDelta);
    float3 G4 = ComputeGradientCD(volume, samplerState, texcoord + float3(+gradientDelta.x, -gradientDelta.y, +gradientDelta.z), gradientDelta);
    float3 G5 = ComputeGradientCD(volume, samplerState, texcoord + float3(-gradientDelta.x, -gradientDelta.y, +gradientDelta.z), gradientDelta);
    float3 G6 = ComputeGradientCD(volume, samplerState, texcoord + float3(+gradientDelta.x, +gradientDelta.y, -gradientDelta.z), gradientDelta);
    float3 G7 = ComputeGradientCD(volume, samplerState, texcoord + float3(-gradientDelta.x, +gradientDelta.y, +gradientDelta.z), gradientDelta);
    float3 G8 = ComputeGradientCD(volume, samplerState, texcoord + float3(+gradientDelta.x, -gradientDelta.y, -gradientDelta.z), gradientDelta);
 
    float3 L0 = lerp(lerp(G1, G2, 0.5), lerp(G3, G4, 0.5), 0.5);
    float3 L1 = lerp(lerp(G5, G6, 0.5), lerp(G7, G8, 0.5), 0.5);

    return lerp(G0, lerp(L0, L1, 0.5), 0.75);
}

float3 GetNormalizedTexcoord(float3 position, AABB aabb) {
    return (position - aabb.Min) / (aabb.Max - aabb.Min);
}
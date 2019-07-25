
#include "Common.hlsl"

static const float M_PI = 3.14159265f;
static const float M_EPSILON = 1.0e-3f;

static const float FLT_EPSILON = 1.192092896e-07f;
static const float FLT_MAX = 3.402823466e+38f;
static const float FLT_MIN = 1.175494351e-38f;

struct Ray {
    float3 Origin;
    float3 Direction;
    float Min;
    float Max;
};

struct Intersection {
    float3 Position;
    float3 Normal;
    float  Near;
    float  Far;
    bool   IsValid;
};

struct ScatterEvent {
    float3 Position;
    float3 Normal;
    float3 View;
    float3 Diffuse;
    float3 Specular;
    float  Roughness;
    float  Alpha;
    float  GradMag;
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

struct VolumeDesc {
    Texture3D<float1> TextureVolumeIntensity;
    Texture3D<float4> TextureVolumeGradient;
    Texture1D<float3> TextureDiffuse;
    Texture1D<float3> TextureSpecular;
    Texture1D<float1> TextureRoughness;
    Texture1D<float1> TextureOpacity;
    AABB              BoundingBox;
    AABB              BoundingBoxCrop;
    float3x3          Rotation;
    float             StepSize;
    float             DensityScale;
    float3            GradientDelta;
};



Texture3D<float1> TextureVolumeIntensity            : register(t0);
Texture3D<float4> TextureVolumeGradient             : register(t1);
Texture1D<float3> TextureTransferFunctionDiffuse    : register(t2);
Texture1D<float3> TextureTransferFunctionSpecular   : register(t3);
Texture1D<float1> TextureTransferFunctionRoughness  : register(t4);
Texture1D<float1> TextureTransferFunctionOpacity    : register(t5);
Texture2D<float3> TextureEnviroment                 : register(t6);

RWTexture2D<float4> TexturePositionUAV : register(u0);
RWTexture2D<float4> TextureNormalUAV   : register(u1);
RWTexture2D<float4> TextureColorUAV    : register(u2);



Texture2D<float4>   TexturePositionSRV   : register(t0);
Texture2D<float4>   TextureNormalSRV     : register(t1);
Texture2D<float4>   TextureColorSRV      : register(t2);

RWTexture2D<float4> TexturePositionSumUAV : register(u0);
RWTexture2D<float4> TextureNormalSumUAV   : register(u1);
RWTexture2D<float4> TextureColorSumUAV    : register(u2);

SamplerState SamplerPoint       : register(s0);
SamplerState SamplerLinear      : register(s1);
SamplerState SamplerAnisotropic : register(s2);



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


Ray CreateCameraRay(uint2 id, CRNG rng)
{
    float2 dimension;
    TextureColorUAV.GetDimensions(dimension.x, dimension.y);

    float2 ncdXY = 2.0f * (id.xy + FrameBuffer.FrameOffset) / dimension - 1.0f;
    ncdXY.y *= -1.0f;

    float4 rayStart = mul(FrameBuffer.InvViewProjection, float4(ncdXY, 0.0f, 1.0f));
    float4 rayEnd   = mul(FrameBuffer.InvViewProjection, float4(ncdXY, 1.0f, 1.0f));

    rayStart.xyz /= rayStart.w;
    rayEnd.xyz   /= rayEnd.w;


    Ray ray;
    ray.Direction = normalize(rayEnd.xyz - rayStart.xyz);
    ray.Origin = rayStart;
    ray.Min = 0;
    ray.Max = length(rayEnd.xyz - rayStart.xyz);;

    return ray;


}



Intersection IntersectAABB(Ray ray, AABB aabb) {

    Intersection intersect;
    intersect.IsValid = false;
    intersect.Position = 0.0f;
    intersect.Normal = 0.0f;
    intersect.Near = 0.0f;
    intersect.Far = FLT_MAX;

    const float3 invR = rcp(ray.Direction);
    const float3 bot = invR * (aabb.Min - ray.Origin);
    const float3 top = invR * (aabb.Max - ray.Origin);
    const float3 tmin = min(top, bot);
    const float3 tmax = max(top, bot);

    const float largestMin = max(max(tmin.x, tmin.y), max(tmin.x, tmin.z));
    const float largestMax = min(min(tmax.x, tmax.y), min(tmax.x, tmax.z));

    if (largestMax < largestMin)
        return intersect;

    intersect.Near = largestMin > 0.0f ? largestMin : 0.0f;
    intersect.Far = largestMax;

    if (intersect.Near < ray.Min || intersect.Near > ray.Max)
        return intersect;

    intersect.IsValid = true;
    intersect.Position = ray.Origin + ray.Direction * intersect.Near;
    
    [unroll]
    for (int i = 0; i < 3; i++) {
        if (intersect.Position[i] <= aabb.Min[i] + M_EPSILON)
            intersect.Normal[i] = -1.0f;

        if (intersect.Position[i] >= aabb.Max[i] - M_EPSILON)
            intersect.Normal[i] = 1.0f;
    }

    return intersect;
}

float3x3 GetTangentSpace(float3 normal) {  
    const float3 helper = abs(normal.x) > 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    const float3 tangent = normalize(cross(normal, helper));
    const float3 binormal = normalize(cross(normal, tangent));
    return float3x3(tangent, binormal, normal);
}

float3 PhaseFunctionHenyeyGreenstain(float3 VdotL, float g) {
    return (1.0f - g * g) / pow(1.0 + g * g - 2.0f * g * VdotL, 1.5) / (4 * M_PI);
}


float3 FresnelSchlick(float3 F0, float VdotH) {
    return F0 + (1.0f - F0) * pow(1.0 - VdotH, 5.0);
}


float GGX_PartialGeometry(float NdotX, float alpha) {
    const float aa = alpha * alpha;
    return 2.0 * NdotX / (NdotX + sqrt(aa + (1.0 - aa) * (NdotX * NdotX)));
}

float GGX_Distribution(float NdotH, float alpha) {
    const float aa = alpha * alpha;
    const float f = (NdotH * aa - NdotH) * NdotH + 1.0;
    return aa / (M_PI * f * f);
}

float3 GGX_SampleHemisphere(float3 normal, float alpha, inout CRNG rng) {

    float2 e = float2(Rand(rng), Rand(rng));

    float phi = 2.0 * M_PI * e.x;
    float cosTheta = sqrt((1.0 - e.y) / (1.0 + alpha * alpha * e.y - e.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    return mul(float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta), GetTangentSpace(normal));
}


float3 UniformSampleHemisphere(float3 normal, inout CRNG rng){

    float2 e = float2(Rand(rng), Rand(rng));

    float phi = 2.0 * M_PI * e.x;
    float cosTheta = e.y;
    float sinTheta = sqrt(max(0.0f, 1.0 - cosTheta * cosTheta));
    return mul(float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta), GetTangentSpace(normal));
}



float3 UniformSampleSphere(inout CRNG rng) {
    float theta = 2.0 * M_PI * Rand(rng);
    float phi = acos(1.0 - 2.0 * Rand(rng));
    return float3(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi));
}


float3 GetNormalizedTexcoord(float3 position, AABB aabb) {
    return (position - aabb.Min) / (aabb.Max - aabb.Min);
}

 


float GetIntensity(VolumeDesc desc, float3 position) {
    return desc.TextureVolumeIntensity.SampleLevel(SamplerLinear, GetNormalizedTexcoord(position, desc.BoundingBox), 0);
}


float3 GetGradient(VolumeDesc desc, float3 position) {
    return desc.TextureVolumeGradient.SampleLevel(SamplerLinear, GetNormalizedTexcoord(position, desc.BoundingBox), 0).xyz;
}


float GetOpacity(VolumeDesc desc, float3 position) {
    return desc.TextureOpacity.SampleLevel(SamplerLinear, GetIntensity(desc, position), 0);
}

float3 GetDiffuse(VolumeDesc desc, float3 position) {
    return desc.TextureDiffuse.SampleLevel(SamplerLinear, GetIntensity(desc, position), 0);
}

float3 GetSpecular(VolumeDesc desc, float3 position) {
    return desc.TextureSpecular.SampleLevel(SamplerLinear, GetIntensity(desc, position), 0);
}

float GetRoughness(VolumeDesc desc, float3 position)
{
    return desc.TextureRoughness.SampleLevel(SamplerLinear, GetIntensity(desc, position), 0);
}

float3 GetEnviroment(float3 direction) {
   const float theta = acos(direction.y) / M_PI;
   const float phi = atan2(direction.x, -direction.z) / -M_PI * 0.5f;
   return TextureEnviroment.SampleLevel(SamplerAnisotropic, float2(phi, theta), 0); 
   
}

float3 refract(float3 i, float3 n, float eta)
{
    eta = 2.0f - eta;
    float cosi = dot(n, i); 
    return (i * eta - n * (-cosi + eta * cosi));
}

ScatterEvent RayMarching(Ray ray, VolumeDesc desc, inout CRNG rng) {

    ScatterEvent event;
    event.IsValid = false;
    event.Normal = 0.0f;
    event.Position = 0.0f;
    event.View = 0.0f;
    event.Diffuse = 0.0f;
    event.Specular = 0.0f;
    event.Roughness = 0.0f;
    event.Alpha = 0.0f;
    event.Opacity = 0.0f;
    event.GradMag = 0.0f;
    
    Ray transformRay = { mul(desc.Rotation, ray.Origin), mul(desc.Rotation, ray.Direction), 0.0, FLT_MAX };
 
    Intersection intersect = IntersectAABB(transformRay, desc.BoundingBoxCrop);
  
    if (!intersect.IsValid)
        return event;
     
    const float minT = max(intersect.Near, transformRay.Min);
    const float maxT = min(intersect.Far, transformRay.Max);
    const float threshold = -log(Rand(rng)) / desc.DensityScale;

    float sum = 0.0f;
    float t = minT + desc.StepSize * Rand(rng);
    float3 position = 0.0f;;
    
    [loop]
    while (sum < threshold) {
        position = transformRay.Origin + t * transformRay.Direction;
        if (t >= maxT)        
            return event;     
          
        sum += desc.DensityScale * GetOpacity(desc, position) * desc.StepSize;         
        t += desc.StepSize;
    }

    float3 diffuse = GetDiffuse(desc, position);
    float3 specular = GetSpecular(desc, position);
    float3 roughness = GetRoughness(desc, position);
    float3 gradient = GetGradient(desc, position);

    event.IsValid = true;
    event.Normal = mul(-normalize(gradient), desc.Rotation);
    event.Position = mul(position, desc.Rotation);
    event.View = -ray.Direction;
    event.Diffuse = diffuse;
    event.Specular = 0.04;
    event.Roughness = roughness;
    event.Alpha = event.Roughness * event.Roughness;
    event.GradMag = length(gradient);
    event.Opacity = GetOpacity(desc, position);

    return event;
    
}


[numthreads(8, 8, 1)]
void FirstPass(uint3 id : SV_DispatchThreadID) {
    
   
   
    CRNG rng = InitCRND(id.xy, FrameBuffer.FrameIndex);
    Ray ray = CreateCameraRay(id.xy, rng);
 
    float3 color = { 0, 0, 0 };
    float3 normal = { 0, 0, 0 };
    float3 position = { 0.0, 0.0, 0.0 };
    float3 energy = { 1, 1, 1 };


    VolumeDesc desc;
    desc.BoundingBox.Min = float3(-FrameBuffer.BoundingBoxSize, -FrameBuffer.BoundingBoxSize, -FrameBuffer.BoundingBoxSize);
    desc.BoundingBox.Max = float3(+FrameBuffer.BoundingBoxSize, +FrameBuffer.BoundingBoxSize, +FrameBuffer.BoundingBoxSize);
    desc.BoundingBoxCrop.Min = FrameBuffer.BoundingBoxMin;
    desc.BoundingBoxCrop.Max = FrameBuffer.BoundingBoxMax;
    desc.StepSize = distance(desc.BoundingBox.Min, desc.BoundingBox.Max) / FrameBuffer.StepCount;
    desc.DensityScale = FrameBuffer.Density;
    desc.Rotation = FrameBuffer.BoundingBoxRotation;
    desc.GradientDelta = (FrameBuffer.BoundingBoxMax - FrameBuffer.BoundingBoxMin) * FrameBuffer.GradientDelta.xyz;
    desc.TextureVolumeIntensity = TextureVolumeIntensity;
    desc.TextureVolumeGradient  = TextureVolumeGradient;
    desc.TextureDiffuse = TextureTransferFunctionDiffuse;
    desc.TextureSpecular = TextureTransferFunctionSpecular;
    desc.TextureRoughness = TextureTransferFunctionRoughness;
    desc.TextureOpacity = TextureTransferFunctionOpacity;
    
    
       
    [loop]
    for (int depth = 0; depth < FrameBuffer.TraceDepth; depth++) {
         
     
         ScatterEvent event = RayMarching(ray, desc, rng);
       
         if (!event.IsValid) {
            color += (depth == 0 && FrameBuffer.IsEnableEnviroment <= 0.0) ? 0.0 : energy * GetEnviroment(ray.Direction);
            color *= (depth != 0) ? FrameBuffer.IBLScale : 1.0f;
            break;
         }

      
        if (depth == 0 && event.IsValid) {
            position = event.Position;
            normal   = event.Normal;
        }
        

       
       
          
      //  const float3 N = event.Normal;
      //  const float3 V = event.View;
      //  const float3 L = GGX_SampleHemisphere(event.Normal, 1.0, rng);
      // 
      //         
      // 
      //  const float NdotL = saturate(dot(N, L));
      // 
      //  ray.Origin = event.Position;
      //  ray.Direction = L;
      //  energy *= 2.0 * event.Diffuse * NdotL;

       
        const float3 N = event.Normal;
        const float3 V = event.View;
        const float3 H = GGX_SampleHemisphere(event.Normal, event.Alpha, rng);
        const float3 F = FresnelSchlick(event.Specular, saturate(dot(V, N)));
        
        
        
       
        const float pd = length(1 - F);
        const float ps = length(F);
        
        const float pdf = ps / (ps + pd);
        
        if (Rand(rng) < pdf) {
        
            const float3 L = reflect(-V, H);
            const float NdotL = saturate(dot(N, L));
            const float NdotV = saturate(dot(N, V));
            const float NdotH = saturate(dot(N, H));
            const float VdotH = saturate(dot(V, H));
                    
            const float G = GGX_PartialGeometry(NdotV, event.Alpha) * GGX_PartialGeometry(NdotL, event.Alpha);
             
            ray.Origin = event.Position;
            ray.Direction = L;
            energy *= (G * F * VdotH) / (NdotV * NdotH + FLT_EPSILON) / (pdf);
        
        } else {
                 
            const float3 L = GGX_SampleHemisphere(N, 1.0, rng);
            const float NdotL = saturate(dot(N, L));
        
            ray.Origin = event.Position;
            ray.Direction = L;
            energy *= 2.0 * saturate((1 - F)) * event.Diffuse * NdotL /(1 - pdf);
        }

              
    }
    TextureColorUAV[id.xy] = float4(color, 1.0);
    TexturePositionUAV[id.xy] = float4(position, 1.0);
    TextureNormalUAV[id.xy] = float4(normal, 1.0f);

   
}

[numthreads(8, 8, 1)]
void SecondPass(uint3 id : SV_DispatchThreadID) {

    TexturePositionSumUAV[id.xy] = float4((TexturePositionSumUAV[id.xy].xyz * FrameBuffer.FrameIndex + TexturePositionSRV[id.xy].xyz) / (FrameBuffer.FrameIndex + 1), 1.0);
    TextureNormalSumUAV[id.xy]   = float4((TextureNormalSumUAV[id.xy].xyz * FrameBuffer.FrameIndex + TextureNormalSRV[id.xy].xyz) / (FrameBuffer.FrameIndex + 1), 1.0);
    TextureColorSumUAV[id.xy]    = float4((TextureColorSumUAV[id.xy].xyz * FrameBuffer.FrameIndex + TextureColorSRV[id.xy].xyz) / (FrameBuffer.FrameIndex + 1), 1.0);
    
   
}
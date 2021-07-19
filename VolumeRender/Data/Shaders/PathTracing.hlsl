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
    AABB BoundingBox;
    float StepSize;
    float DensityScale;
};

Texture3D<float> TextureVolumeIntensity : register(t0);
Texture1D<float3> TextureTransferFunctionDiffuse : register(t1);
Texture1D<float3> TextureTransferFunctionSpecular : register(t2);
Texture1D<float1> TextureTransferFunctionRoughness : register(t3);
Texture1D<float1> TextureTransferFunctionOpacity : register(t4);
Texture2D<float3> TextureEnvironment : register(t5);

RWTexture2D<float4> TextureColorUAV : register(u0);
Texture2D<float4> TextureColorSRV : register(t0);
RWTexture2D<float4> TextureColorSumUAV : register(u0);

SamplerState SamplerPoint : register(s0);
SamplerState SamplerLinear : register(s1);
SamplerState SamplerAnisotropic : register(s2);

float3x3 GetTangentSpace(float3 normal) {
    const float3 helper = abs(normal.x) > 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    const float3 tangent = normalize(cross(normal, helper));
    const float3 binormal = normalize(cross(normal, tangent));
    return float3x3(tangent, binormal, normal);
}

float PhaseFunctionHenyeyGreenstain(float phi, float g) {
    return (1.0f - g * g) / pow(1.0 + g * g - 2.0f * g * phi, 1.5);
}

float3 FresnelSchlick(float3 F0, float VdotH) {
    return F0 + (1.0f - F0) * pow(1.0 - VdotH, 5.0);
}

float GGX_PartialGeometry(float NdotX, float alpha) {
    const float aa = alpha * alpha;
    return 2.0 * NdotX / max((NdotX + sqrt(aa + (1.0 - aa) * (NdotX * NdotX))), FLT_EPSILON);
}

float GGX_Distribution(float NdotH, float alpha) {
    const float aa = alpha * alpha;
    const float f = (NdotH * aa - NdotH) * NdotH + 1.0;
    return aa / (M_PI * f * f);
}

float3 GGX_SampleHemisphere(float3 normal, float alpha, inout CRNG rng) {
    float2 e = float2(Rand(rng), Rand(rng));
    float phi = 2.0 * M_PI * e.x;
    float cosTheta = sqrt(max(0.0f, (1.0 - e.y) / (1.0 + alpha * alpha * e.y - e.y)));
    float sinTheta = sqrt(max(0.0f, 1.0 - cosTheta * cosTheta));
    return mul(float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta), GetTangentSpace(normal));
}

float3 UniformSampleHemisphere(float3 normal, inout CRNG rng) {
    float2 e = float2(Rand(rng), Rand(rng));
    float phi = 2.0 * M_PI * e.x;
    float cosTheta = e.y;
    float sinTheta = sqrt(max(0.0f, 1.0 - cosTheta * cosTheta));
    return mul(float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta), GetTangentSpace(normal));
}

float3 UniformSampleSphere(inout CRNG rng) {
    float theta = 2.0f * M_PI * Rand(rng);
    float phi = acos(1.0 - 2.0 * Rand(rng));
    return float3(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi));
}

float GetIntensity(VolumeDesc desc, float3 position) {
    return TextureVolumeIntensity.SampleLevel(SamplerLinear, GetNormalizedTexcoord(position, desc.BoundingBox), 0);
}

float3 GetGradient(VolumeDesc desc, float3 position) {
    int3 dimension;
    TextureVolumeIntensity.GetDimensions(dimension.x, dimension.y, dimension.z);
    return ComputeGradientFiltered(TextureVolumeIntensity, SamplerLinear, GetNormalizedTexcoord(position, desc.BoundingBox), float3(1.0 / dimension.x, 1.0 / dimension.y, 1.0 / dimension.z));
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

float3 GetEnvironment(float3 direction) {
    const float theta = acos(direction.y) / M_PI;
    const float phi = atan2(direction.x, -direction.z) / M_PI * 0.5f;
    return TextureEnvironment.SampleLevel(SamplerAnisotropic, float2(phi, theta), 0);  
}

ScatterEvent RayMarching(Ray ray, VolumeDesc desc, inout CRNG rng) {
    ScatterEvent event;  
    event.Position = float3(0.0f, 0.0f, 0.0f);
    event.Normal = float3(0.0f, 0.0f, 0.0f);
    event.View = float3(0.0f, 0.0f, 0.0f);
    event.Diffuse = float3(0.0f, 0.0f, 0.0f);
    event.Specular = float3(0.0f, 0.0f, 0.0f);
    event.Roughness = 0.0f;
    event.Opacity = 0.0f;
    event.Alpha = 0.0f;
    event.IsValid = false;
   
    Intersection intersect = IntersectAABB(ray, desc.BoundingBox);
	
    [branch]
    if (!intersect.IsValid)
        return event;

    const float minT = max(intersect.Near, ray.Min);
    const float maxT = min(intersect.Far, ray.Max);
    const float threshold = -log(Rand(rng)) / desc.DensityScale;
	
    float sum = 0.0f;
    float t = minT + Rand(rng) * desc.StepSize;
    float3 position = float3(0.0f, 0.0f, 0.0f);
		
    [loop]
    while (sum < threshold) {
        position = ray.Origin + t * ray.Direction;
        [branch]
        if (t >= maxT)
            return event;
	
        sum += desc.DensityScale * GetOpacity(desc, position) * desc.StepSize;
        t += desc.StepSize;
    }
	
    float3 diffuse = GetDiffuse(desc, position);
    float3 specular = GetSpecular(desc, position);
    float roughness = GetRoughness(desc, position);
    float3 gradient = GetGradient(desc, position);
	
    [branch]
    if (length(gradient) < FLT_EPSILON) 
        return event;
    
    event.IsValid = true;
    event.Normal = -normalize(gradient);
    event.Normal = dot(event.Normal, -ray.Direction) < 0.0f ? -event.Normal : event.Normal;
    event.Position = position + 0.001f * event.Normal;
    event.View = -ray.Direction;
    event.Diffuse = diffuse;
    event.Specular = specular;
    event.Roughness = roughness;
    event.Alpha = roughness * roughness;
    event.Opacity = GetOpacity(desc, position);

    return event;
}

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void RayTracing(uint3 id : SV_DispatchThreadID) {
    CRNG rng = InitCRND(id.xy, FrameBuffer.FrameIndex);
    Ray ray = CreateCameraRay(id.xy, FrameBuffer.FrameOffset, FrameBuffer.RenderTargetDim);
 	
    ray.Origin = mul(FrameBuffer.InvWorldMatrix, float4(ray.Origin, 1.0)).xyz;
    ray.Direction = mul((float3x3) FrameBuffer.InvNormalMatrix, ray.Direction);
    
    float3 radiance = { 0.0f, 0.0f, 0.0f };
    float3 throughput = { 1.0f, 1.0f, 1.0f };

    VolumeDesc desc;
    desc.BoundingBox.Min = FrameBuffer.BoundingBoxMin;
    desc.BoundingBox.Max = FrameBuffer.BoundingBoxMax;
    desc.StepSize = distance(desc.BoundingBox.Min, desc.BoundingBox.Max) / FrameBuffer.StepCount;
    desc.DensityScale = FrameBuffer.Density;
       
    for (uint depth = 0; depth < FrameBuffer.TraceDepth; depth++) {
        ScatterEvent event = RayMarching(ray, desc, rng);
		
        [branch]
        if (!event.IsValid) {
            radiance += (depth == 0) ? 0.0 : throughput * GetEnvironment(mul((float3x3) FrameBuffer.NormalMatrix, ray.Direction));
            break;
        }
				
        const float3 N = event.Normal;
        const float3 V = event.View;
		
        const float3 H = GGX_SampleHemisphere(event.Normal, event.Alpha, rng);
        const float3 F = FresnelSchlick(event.Specular, saturate(dot(V, H)));
					
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
            throughput *= (G * F * VdotH) / (NdotV * NdotH + M_EPSILON) / (pdf);
		
        } else {
            const float3 L = GGX_SampleHemisphere(N, 1.0, rng);
            ray.Origin = event.Position;
            ray.Direction = L;
            throughput *= saturate((1 - F)) * event.Diffuse / (1 - pdf);
        }
    }
    
    TextureColorUAV[id.xy] = float4(radiance, 1.0);
}

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void FrameSum(uint3 id : SV_DispatchThreadID) {
    float alpha = 1.0f / (FrameBuffer.FrameIndex + 1.0f);
    TextureColorSumUAV[id.xy] = lerp(TextureColorSumUAV[id.xy], TextureColorSRV[id.xy], float4(alpha, alpha, alpha, alpha));
}

#pragma once

#pragma pack_matrix( row_major )

cbuffer ConstantFrameBuffer : register(b0)
{
    struct
    {
  
        float4x4 ViewProjection;
        float4x4 WorldMatrix;
        float4x4 NormalMatrix;

        float4x4 InvViewProjection;
        float4x4 InvWorldMatrix;
        float4x4 InvNormalMatrix;

        float BoundingBoxSize;
        uint  FrameIndex;
        uint  TraceDepth;
        uint  StepCount;

        float  Density;
        float3 BoundingBoxMin;

        float  Exposure;
        float3 BoundingBoxMax;
       

        float3 GradientDelta;
        float  DenoiserStrange;

        float  IsEnableEnviroment;
        float  LightIntensity;
        float2 FrameOffset;
    } FrameBuffer;
}
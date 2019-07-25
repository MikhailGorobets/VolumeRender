
#pragma once

cbuffer ConstantFrameBuffer : register(b0)
{
    struct
    {
  
        float4x4 InvViewProjection;
        float4x4 BoundingBoxRotation;

        float BoundingBoxSize;
        uint  FrameIndex;
        uint  TraceDepth;
        uint  StepCount;

        float  Density;
        float3 BoundingBoxMin;

        float  Exposure;
        float3 BoundingBoxMax;
       
      
        float  IBLScale;
        float3 _Padding;

        float3 GradientDelta;
        float  DenoiserStrange;

        float  IsEnableEnviroment;
        float  Gamma;
        float2 FrameOffset;
    } FrameBuffer;
}
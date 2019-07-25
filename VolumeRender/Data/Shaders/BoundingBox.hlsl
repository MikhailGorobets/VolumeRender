#include "Common.hlsl"


struct InputVS {
    float3 Position : POSITION;
};



void VS_Main(InputVS input,  out float4 position : SV_Position)
{
    position = float4(input.Position * abs(FrameBuffer.BoundingBoxMin), 1.0f);
    position = mul(position, FrameBuffer.BoundingBoxRotation);
  //  position = mul(position, FrameBuffer.CameraView);
  //  position = mul(position, FrameBuffer.CameraProj);
    

}

float4 PS_Main(float4 position : SV_Position) : SV_TARGET0
{
    return float4(0.0, 1.0, 0.0, 1.0f);
}
Texture2D<float4>   TextureHDR : register(t0);
RWTexture2D<float4> TextureLDR : register(u0);

cbuffer ConstantFrameBuffer : register(b0)
{
    struct
    {
        float4x4 CameraView;
        float4x4 BoundingBoxRotation;

        float CameraFocalLenght;
        float3 CameraOrigin;
       
        float CameraAspectRatio;
        uint FrameIndex;
        uint TraceDepth;
        uint StepCount;

        float Density;
        float3 BoundingBoxMin;

        float Exposure;
        float3 BoundingBoxMax;
       
        float4 GradientDelta;
        float  IsEnableEnviroment;
        float  Gamma;
        float2 FrameOffset;
    } FrameBuffer;
}


float3 Uncharted2Function(float A, float B, float C, float D, float E, float F, float3 x) {
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 ToneMapUncharted2Function(float3 x, float exposure) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;
    const float W = 11.2;

    float3 color = Uncharted2Function(A, B, C, D, E, F, x * exposure);
    float3 white = Uncharted2Function(A, B, C, D, E, F, W);

    return color / white;

}


float3 LinearToSRGB(float3 color) {
    float3 sRGBLo = color * 12.92;
    const float powExp = 1.0 / FrameBuffer.Gamma;
    float3 sRGBHi = (pow(abs(color), float3(powExp, powExp, powExp)) * 1.055) - 0.055;
    float3 sRGB;
    sRGB.x = (color.x <= 0.0031308) ? sRGBLo.x : sRGBHi.x;
    sRGB.y = (color.y <= 0.0031308) ? sRGBLo.y : sRGBHi.y;
    sRGB.z = (color.z <= 0.0031308) ? sRGBLo.z : sRGBHi.z;
    return sRGB;
}



[numthreads(8, 8, 1)]
void ToneMap(uint3 id : SV_DispatchThreadID) {
    float3 colorHDR = TextureHDR.Load(int3(id.xy, 0)).xyz;
    TextureLDR[id.xy] = float4(LinearToSRGB(ToneMapUncharted2Function(colorHDR, FrameBuffer.Exposure)), 1.0f);
}
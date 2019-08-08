
#include "Common.hlsl"



Texture2D<float4> TexturePosition  : register(t0);
Texture2D<float4> TextureNormal    : register(t1);
Texture2D<float4> TextureColor     : register(t2);



RWTexture2D<float4> TextureResult: register(u0);



[numthreads(8, 8, 1)]
void Denoiser(uint3 id : SV_DispatchThreadID)
{
    

    float2 offset[25];
    offset[0] = float2(-2, -2);
    offset[1] = float2(-1, -2);
    offset[2] = float2(0, -2);
    offset[3] = float2(1, -2);
    offset[4] = float2(2, -2);
    
    offset[5] = float2(-2, -1);
    offset[6] = float2(-1, -1);
    offset[7] = float2(0, -1);
    offset[8] = float2(1, -1);
    offset[9] = float2(2, -1);
    
    offset[10] = float2(-2, 0);
    offset[11] = float2(-1, 0);
    offset[12] = float2(0, 0);
    offset[13] = float2(1, 0);
    offset[14] = float2(2, 0);
    
    offset[15] = float2(-2, 1);
    offset[16] = float2(-1, 1);
    offset[17] = float2(0, 1);
    offset[18] = float2(1, 1);
    offset[19] = float2(2, 1);
    
    offset[20] = float2(-2, 2);
    offset[21] = float2(-1, 2);
    offset[22] = float2(0, 2);
    offset[23] = float2(1, 2);
    offset[24] = float2(2, 2);
    
    
    float kernel[25];
    kernel[0] = 1.0f / 256.0f;
    kernel[1] = 1.0f / 64.0f;
    kernel[2] = 3.0f / 128.0f;
    kernel[3] = 1.0f / 64.0f;
    kernel[4] = 1.0f / 256.0f;
    
    kernel[5] = 1.0f / 64.0f;
    kernel[6] = 1.0f / 16.0f;
    kernel[7] = 3.0f / 32.0f;
    kernel[8] = 1.0f / 16.0f;
    kernel[9] = 1.0f / 64.0f;
    
    kernel[10] = 3.0f / 128.0f;
    kernel[11] = 3.0f / 32.0f;
    kernel[12] = 9.0f / 64.0f;
    kernel[13] = 3.0f / 32.0f;
    kernel[14] = 3.0f / 128.0f;
    
    kernel[15] = 1.0f / 64.0f;
    kernel[16] = 1.0f / 16.0f;
    kernel[17] = 3.0f / 32.0f;
    kernel[18] = 1.0f / 16.0f;
    kernel[19] = 1.0f / 64.0f;
    
    kernel[20] = 1.0f / 256.0f;
    kernel[21] = 1.0f / 64.0f;
    kernel[22] = 3.0f / 128.0f;
    kernel[23] = 1.0f / 64.0f;
    kernel[24] = 1.0f / 256.0f;
    

    float4 sumColor = 0.0;
    float  sumWeight = 0.0f;

    float PPhi = 0.05;
    float NPhi = 0.05;
    float CPhi = 0.1;
  

    float4 P = TexturePosition.Load(int3(id.xy, 0));
    float4 N = TextureNormal.Load(int3(id.xy, 0));
    float4 C = TextureColor.Load(int3(id.xy, 0));
  
 
    
    
    for (int index = 0; index < 25; index++) {

        float4 PTemp = TexturePosition.Load(int3(id.xy + FrameBuffer.DenoiserStrange * offset[index], 0));
        float4 NTemp = TextureNormal.Load(int3(id.xy + FrameBuffer.DenoiserStrange * offset[index], 0));
        float4 CTemp = TextureColor.Load(int3(id.xy + FrameBuffer.DenoiserStrange * offset[index], 0));
      
        
        float4 PT = dot(P - PTemp, P - PTemp);
        float4 NT = max(dot(N - NTemp, N - NTemp), 0.0);
        float4 CT = dot(C - CTemp, C - CTemp);
      
       
        float PW = min(exp(-(PT) / PPhi), 1.0);
        float NW = min(exp(-(NT) / NPhi), 1.0);
        float CW = min(exp(-(CT) / CPhi), 1.0);
     
  
        float weight = CW * NW * PW;
        sumColor  += CTemp * weight * kernel[index];
        sumWeight += weight * kernel[index];
    }

    TextureResult[id.xy] = float4(sumColor / sumWeight);

}
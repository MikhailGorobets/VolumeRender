#include "Common.hlsl"

#define WAVEFRONT_SIZE 64

Texture2D<float3>            TextureColorSRV: register(t0);
Texture2D<float3>            TextureNormalSRV: register(t1);
Texture2D<float>             TextureDepthSRV: register(t2);
AppendStructuredBuffer<uint> BufferTiles: register(u0);

groupshared float SharedBuffer[WAVEFRONT_SIZE];

float Luminance(float3 color) {
    return dot(float3(0.2126, 0.7152, 0.0722), color);
}

float ComputeAverage(uint lineID) {
    for (uint stride = WAVEFRONT_SIZE / 2; stride > 0; stride = stride >> 1) {
        if (lineID < stride) {
            SharedBuffer[lineID] += SharedBuffer[lineID + stride];
            GroupMemoryBarrier();
        }     
    }  
    return SharedBuffer[0] / WAVEFRONT_SIZE;
}

float ComputeDispersion(uint3 thredID, uint lineID, Texture2D<float3> sourceTexture) {
    SharedBuffer[lineID] = Luminance(sourceTexture[thredID.xy]);
    GroupMemoryBarrierWithGroupSync(); 
    float expected = ComputeAverage(lineID);
    
    //TODO(SM 6.0+) WaveActiveSum 
    SharedBuffer[lineID] = distance(expected, Luminance(sourceTexture[thredID.xy]));
    GroupMemoryBarrierWithGroupSync();
    return ComputeAverage(lineID);;
}

float ComputeDispersion(uint3 thredID, uint lineID, Texture2D<float> sourceTexture) {
    SharedBuffer[lineID] = sourceTexture[thredID.xy];
    GroupMemoryBarrierWithGroupSync();
    float expected = ComputeAverage(lineID);
    
    //TODO(SM 6.0+) WaveActiveSum 
    SharedBuffer[lineID] = distance(expected, sourceTexture[thredID.xy]);
    GroupMemoryBarrierWithGroupSync();
    return ComputeAverage(lineID);;
}

[numthreads(8, 8, 1)]
void Dispersion(uint3 thredID: SV_DispatchThreadID, uint lineID: SV_GroupIndex, uint3 groupID: SV_GroupID) {
    const uint tileID = (0xFFFF & groupID.x) | ((0xFFFF & groupID.y) << 16);
       
    float dispersionColor  = 0.50 * ComputeDispersion(thredID, lineID, TextureColorSRV);
    float dispersionNormal = 0.25 * ComputeDispersion(thredID, lineID, TextureNormalSRV);
    float dispersionDepth  = 0.25 * ComputeDispersion(thredID, lineID, TextureDepthSRV);
    float dispersionTotal = dispersionColor + dispersionNormal + dispersionDepth;
     
    if (dispersionTotal > FrameBuffer.Dispersion && lineID == 0)
        BufferTiles.Append(tileID);
}
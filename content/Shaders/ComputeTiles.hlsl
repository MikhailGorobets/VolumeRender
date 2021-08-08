#include "Common.hlsl"

#define WAVEFRONT_SIZE (THREAD_GROUP_SIZE_X) * (THREAD_GROUP_SIZE_Y)

Texture2D<float3>            TextureColorSRV: register(t0);
Texture2D<float>             TextureDepthSRV: register(t1);
AppendStructuredBuffer<uint> BufferTiles: register(u0);

groupshared float SharedBuffer[WAVEFRONT_SIZE];

float Luminance(float3 color) {
    return dot(float3(0.2126, 0.7152, 0.0722), color);
}

float ReductionSum(uint lineID) {
    [unrool(WAVEFRONT_SIZE / 2)]
    for (uint stride = WAVEFRONT_SIZE / 2; stride > 0; stride = stride >> 1) {
        if (lineID < stride) 
            SharedBuffer[lineID] += SharedBuffer[lineID + stride];
    }  
    return SharedBuffer[0];
}

float ComputeSum(uint3 thredID, uint lineID, Texture2D<float3> sourceTexture) {
    SharedBuffer[lineID] = Luminance(sourceTexture[thredID.xy]);
    return ReductionSum(lineID);
}

float ComputeSum(uint3 thredID, uint lineID, Texture2D<float> sourceTexture) {
    SharedBuffer[lineID] = sourceTexture[thredID.xy];
    return ReductionSum(lineID);
}

[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void ComputeTiles(uint3 thredID: SV_DispatchThreadID, uint lineID: SV_GroupIndex, uint3 groupID: SV_GroupID) {
    const uint tileID = (0xFFFF & groupID.x) | ((0xFFFF & groupID.y) << 16);
       
    float colorSum = 0.75 * ComputeSum(thredID, lineID, TextureColorSRV);
    float depthSum = 0.25 * ComputeSum(thredID, lineID, TextureDepthSRV);
    float totalSum = colorSum + depthSum;
     
    if (totalSum > 0 && lineID == 0)
        BufferTiles.Append(tileID);
}
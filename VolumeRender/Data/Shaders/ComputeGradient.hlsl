Texture3D<float>    TextureIntensity;
RWTexture3D<float4> TextureGradient;



float3 ComputeGradientCD(Texture3D<float> volume, int3 position) {
      
    const float dx = volume.Load(int4(position, 0) + int4(1, 0, 0, 0)) - volume.Load(int4(position, 0) - int4(1, 0, 0, 0));
    const float dy = volume.Load(int4(position, 0) + int4(0, 1, 0, 0)) - volume.Load(int4(position, 0) - int4(0, 1, 0, 0));
    const float dz = volume.Load(int4(position, 0) + int4(0, 0, 1, 0)) - volume.Load(int4(position, 0) - int4(0, 0, 1, 0));
                   
    return float3(dx, dy, dz);
}

float3 ComputeGradientFD(Texture3D<float> volume, int3 position) { 

    const float p  = volume.Load(int4(position, 0));
    const float dx = volume.Load(int4(position, 0) + int4(1, 0, 0, 0)) - p;
    const float dy = volume.Load(int4(position, 0) + int4(0, 1, 0, 0)) - p;
    const float dz = volume.Load(int4(position, 0) + int4(0, 0, 1, 0)) - p;

    return float3(dx, dy, dz);
}



float3 ComputeGradientFiltered(Texture3D<float> volume, int3 position) {
    const float3 G0 = ComputeGradientCD(volume, position);
    const float3 G1 = ComputeGradientCD(volume, position + float3(-1, -1, -1));
    const float3 G2 = ComputeGradientCD(volume, position + float3( 1,  1,  1));
    const float3 G3 = ComputeGradientCD(volume, position + float3(-1,  1, -1));
    const float3 G4 = ComputeGradientCD(volume, position + float3( 1, -1,  1));
    const float3 G5 = ComputeGradientCD(volume, position + float3(-1, -1,  1));
    const float3 G6 = ComputeGradientCD(volume, position + float3( 1,  1, -1));
    const float3 G7 = ComputeGradientCD(volume, position + float3(-1,  1,  1));
    const float3 G8 = ComputeGradientCD(volume, position + float3( 1, -1, -1));
    
    const float3 L0 = lerp(lerp(G1, G2, 0.5), lerp(G3, G4, 0.5), 0.5);
    const float3 L1 = lerp(lerp(G5, G6, 0.5), lerp(G7, G8, 0.5), 0.5);
    
    return lerp(G0, lerp(L0, L1, 0.5), 0.75);
}



[numthreads(8, 8, 8)]
void ComputeGradient(uint3 id : SV_DispatchThreadID) {
    TextureGradient[id] = float4(ComputeGradientFiltered(TextureIntensity, id), TextureIntensity.Load(int4(id, 0)));
}


SamplerState       LinearSampler: register(s0);
Texture3D<float>   TextureSrc: register(t0);
RWTexture3D<float> TextureDst: register(u0);

float Mip(uint3 position) {
    float3 dimension;
    TextureSrc.GetDimensions(dimension.x, dimension.y, dimension.z);
    float3 texcoord = (position.xyz + float3(0.5, 0.5, 0.5)) / dimension;
    return TextureSrc.SampleLevel(LinearSampler, texcoord, 0);
}

[numthreads(4, 4, 4)]
void GenerateMipLevel(uint3 thredID: SV_DispatchThreadID, uint lineID: SV_GroupIndex) {
    TextureDst[thredID] = Mip(thredID);
}
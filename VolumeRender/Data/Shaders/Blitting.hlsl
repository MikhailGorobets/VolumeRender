Texture2D<float4> TextureSrc  : register(t0);
SamplerState      SamplerPoint : register(s0);

void BlitVS(uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD) {
    texcoord = float2((id << 1) & 2, id & 2);
    position = float4(texcoord * float2(2, -2) + float2(-1, 1), 1, 1);
}

float4 BlitPS(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_TARGET0 {
    return TextureSrc.Sample(SamplerPoint, texcoord);
}
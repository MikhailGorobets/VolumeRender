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


float Consovolution(float3x3 x, float3x3 y) {

    float sum = 0;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            sum += x[i][j] * y[i][j];

    return sum;
}


float ComputeGradientSobelX(float3x3 slice1, float3x3 slice2, float3x3 slice3) {

    float3x3 kernel1 = {
        -1, -3, -1,
        -3, -6, -3,
        -1, -3, -1
    };

    float3x3 kernel2 = {
        0, 0, 0,
        0, 0, 0,
        0, 0, 0
    };

    float3x3 kernel3 = {
        1, 3, 1,
        3, 6, 3,
        1, 3, 1
    };

    float x = Consovolution(slice1, kernel1);
    float y = Consovolution(slice2, kernel2);
    float z = Consovolution(slice3, kernel3);

    return (x + y + z) / 22;
}

float ComputeGradientSobelY(float3x3 slice1, float3x3 slice2, float3x3 slice3) {

    float3x3 kernel1 = {
        1,  3,  1,
        0,  0,  0,
       -1, -3, -1
    };

    float3x3 kernel2 = {
        3,  6,  3,
        0,  0,  0,
       -3, -6, -3
    };

    float3x3 kernel3 = {
        1,  3,  1,
         0,  0,  0,
        -1, -3, -1
    };

    
    float x = Consovolution(slice1, kernel1);
    float y = Consovolution(slice2, kernel2);
    float z = Consovolution(slice3, kernel3);

    return (x + y + z) / 22;


}

float ComputeGradientSobelZ(float3x3 slice1, float3x3 slice2, float3x3 slice3) {

    float3x3 kernel1 = {
        -1, 0, 1,
        -3, 0, 3,
        -1, 0, 1
    };

    float3x3 kernel2 = {
        -3,  0,  3,
        -6,  0,  6,
        -3,  0,  3
    };

    float3x3 kernel3 = {
        -1, 0, 1,
        -3, 0, 3,
        -1, 0, 1
    };
   
    float x = Consovolution(slice1, kernel1);
    float y = Consovolution(slice2, kernel2);
    float z = Consovolution(slice3, kernel3);

    return (x + y + z) / 22;
  
}

float3 ComputeGradientSobel(Texture3D<float> volume, int3 position) {
     

   
     float3x3 slice1 = {
         volume.Load(int4(position, 0) + int4(-1,-1, -1, 0)), volume.Load(int4(position, 0) + int4(0, -1, -1, 0)), volume.Load(int4(position, 0) + int4(1, -1,  -1, 0)),
         volume.Load(int4(position, 0) + int4(-1, 0, -1, 0)), volume.Load(int4(position, 0) + int4(0,  0, -1, 0)), volume.Load(int4(position, 0) + int4(1,  0,  -1, 0)),
         volume.Load(int4(position, 0) + int4(-1, 1, -1, 0)), volume.Load(int4(position, 0) + int4(0,  1, -1, 0)), volume.Load(int4(position, 0) + int4(1,  1,  -1, 0))
     };
   
     float3x3 slice2 = {
         volume.Load(int4(position, 0) + int4(-1, -1, 0, 0)), volume.Load(int4(position, 0) + int4(0, -1, 0, 0)), volume.Load(int4(position, 0) + int4(1, -1, 0, 0)),
         volume.Load(int4(position, 0) + int4(-1, 0, 0, 0)),  volume.Load(int4(position, 0) + int4(0, 0, 0, 0)),  volume.Load(int4(position, 0) + int4(1, 0, 0, 0)),
         volume.Load(int4(position, 0) + int4(-1, 1, 0, 0)),  volume.Load(int4(position, 0) + int4(0, 1, 0, 0)),  volume.Load(int4(position, 0) + int4(1, 1, 0, 0))
     };
   
     float3x3 slice3 = {
         volume.Load(int4(position, 0) + int4(-1, -1, 1, 0)), volume.Load(int4(position, 0) + int4(0, -1, 1, 0)), volume.Load(int4(position, 0) + int4(1, -1, 1, 0)),
         volume.Load(int4(position, 0) + int4(-1, 0, 1, 0)),  volume.Load(int4(position, 0) + int4(0, 0, 1, 0)),  volume.Load(int4(position, 0) + int4(1, 0, 1, 0)),
         volume.Load(int4(position, 0) + int4(-1, 1, 1, 0)),  volume.Load(int4(position, 0) + int4(0, 1, 1, 0)),  volume.Load(int4(position, 0) + int4(1, 1, 1, 0))
     };

   
  

                  
   

  //  float x = ComputeGradientSobelX(slice1, slice2, slice3);
  //  float y = ComputeGradientSobelY(slice1, slice2, slice3);
  //  float z = ComputeGradientSobelZ(slice1, slice2, slice3);
   
  
   
    int N = 1;
    float3 sum = 0.0;
    for (int x = -N; x <= N; x++)
        for (int y = -N; y <= N; y++)
            for (int z = -N; z <= N; z++)
                sum += ComputeGradientCD(volume, position + int3(x, y, z));
   
      
    return sum / 27;

}



[numthreads(8, 8, 8)]
void ComputeGradient(uint3 id : SV_DispatchThreadID)
{
    TextureGradient[id] = float4(ComputeGradientFiltered(TextureIntensity, id), TextureIntensity.Load(int4(id, 0)));
}

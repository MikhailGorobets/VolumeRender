/*
 * MIT License
 *
 * Copyright(c) 2021 Mikhail Gorobets
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this softwareand associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions :
 *
 * The above copyright noticeand this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "ApplicationVolumeRender.h"
#include <DirectXTex/DDSTextureLoader.h>
#include <ImGui/imgui.h>
#include <nlohmann/json.hpp>

struct FrameBuffer {
    Hawk::Math::Mat4x4 ViewProjectionMatrix;
    Hawk::Math::Mat4x4 WorldMatrix;
    Hawk::Math::Mat4x4 NormalMatrix;

    Hawk::Math::Mat4x4 InvViewProjectionMatrix;
    Hawk::Math::Mat4x4 InvWorldMatrix;
    Hawk::Math::Mat4x4 InvNormalMatrix;

    F32 DenoiserStrange;
    uint32_t FrameIndex;
    uint32_t TraceDepth;
    uint32_t StepCount;

    F32 Density;
    Hawk::Math::Vec3 BoundingBoxMin;

    F32 Exposure;
    Hawk::Math::Vec3 BoundingBoxMax;

    Hawk::Math::Vec2 FrameOffset;
    Hawk::Math::Vec2 RenderTargetDim;
};

ApplicationVolumeRender::ApplicationVolumeRender(ApplicationDesc const& desc)
    : Application(desc)
    , m_RandomGenerator(m_RandomDevice())
    , m_RandomDistribution(0.0f, 1.0f) {

    this->InitializeShaders();
    this->InitializeVolumeTexture();
    this->InitializeTransferFunction();
    this->InitializeSamplerStates();
    this->InitializeRenderTextures();
    this->InitializeConstantBuffers();
    this->InitializeEnviromentMap();
}

auto ApplicationVolumeRender::InitializeShaders() -> void {
   
    auto compileShader = [](auto fileName, auto entrypoint, auto target, auto macros) -> DX::ComPtr<ID3DBlob> {
        DX::ComPtr<ID3DBlob> pCodeBlob;
        DX::ComPtr<ID3DBlob> pErrorBlob;
      
        if (FAILED(D3DCompileFromFile(fileName, macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, entrypoint, target, D3DCOMPILE_PREFER_FLOW_CONTROL | D3DCOMPILE_ENABLE_STRICTNESS, 0, pCodeBlob.GetAddressOf(), pErrorBlob.GetAddressOf())))
            throw std::runtime_error(static_cast<const char*>(pErrorBlob->GetBufferPointer()));
        return pCodeBlob;
    };

    std::string threadSizeX = std::to_string(m_ThreadSizeDimensionX);
    std::string threadSizeY = std::to_string(m_ThreadSizeDimensionY);

    D3D_SHADER_MACRO macros[] = {
        {"THREAD_GROUP_SIZE_X", threadSizeX.c_str()},
        {"THREAD_GROUP_SIZE_Y", threadSizeY.c_str()},
        { nullptr, nullptr}
    }; 

    auto pBlobCSPathTracing = compileShader(L"Data/Shaders/PathTracing.hlsl", "RayTracing", "cs_5_0", macros);
    auto pBlobCSPathTracingSum = compileShader(L"Data/Shaders/PathTracing.hlsl", "FrameSum", "cs_5_0", macros);
    auto pBlobCSToneMap = compileShader(L"Data/Shaders/ToneMap.hlsl", "ToneMap", "cs_5_0", macros);
    auto pBlobVSBlit = compileShader(L"Data/Shaders/Blitting.hlsl", "BlitVS", "vs_5_0", nullptr);
    auto pBlobPSBlit = compileShader(L"Data/Shaders/Blitting.hlsl", "BlitPS", "ps_5_0", nullptr);

    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSPathTracing->GetBufferPointer(), pBlobCSPathTracing->GetBufferSize(), nullptr, m_PSOPathTracing.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSPathTracingSum->GetBufferPointer(), pBlobCSPathTracingSum->GetBufferSize(), nullptr, m_PSOPathTracingSum.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSToneMap->GetBufferPointer(), pBlobCSToneMap->GetBufferSize(), nullptr, m_PSOToneMap.pCS.ReleaseAndGetAddressOf()));

    DX::ThrowIfFailed(m_pDevice->CreateVertexShader(pBlobVSBlit->GetBufferPointer(), pBlobVSBlit->GetBufferSize(), nullptr, m_PSOBlit.pVS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreatePixelShader(pBlobPSBlit->GetBufferPointer(), pBlobPSBlit->GetBufferSize(), nullptr, m_PSOBlit.pPS.ReleaseAndGetAddressOf()));
    m_PSOBlit.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

auto ApplicationVolumeRender::InitializeVolumeTexture() -> void {
    std::ifstream file("Data/Textures/manix.dat", std::ifstream::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open file: " + std::string("Data/Textures/manix.dat"));

    file.read(reinterpret_cast<char*>(&m_DimensionX), sizeof(uint16_t));
    file.read(reinterpret_cast<char*>(&m_DimensionY), sizeof(uint16_t));
    file.read(reinterpret_cast<char*>(&m_DimensionZ), sizeof(uint16_t));

    std::vector<uint16_t> data(size_t(m_DimensionX) * size_t(m_DimensionY) * size_t(m_DimensionZ));
    std::vector<uint16_t> intensity(size_t(m_DimensionX) * size_t(m_DimensionY) * size_t(m_DimensionZ));

    uint16_t tmin = std::numeric_limits<uint16_t>::max();
    uint16_t tmax = std::numeric_limits<uint16_t>::min();

    auto t1 = std::chrono::high_resolution_clock::now();
    for (auto index = 0u; index < std::size(data); index++) {
        file.read(reinterpret_cast<char*>(&data[index]), sizeof(uint16_t));
        tmin = std::min(tmin, data[index]);
        tmax = std::max(tmax, data[index]);
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

    for (size_t index = 0u; index < std::size(intensity); index++) {
       intensity[index] = static_cast<uint16_t>(std::ceil(std::numeric_limits<uint16_t>::max() * ((data[index] - tmin) / static_cast<F32>(tmax - tmin))));
    }
       
    {
        D3D11_TEXTURE3D_DESC desc = {};
        desc.Width = m_DimensionX;
        desc.Height = m_DimensionY;
        desc.Depth = m_DimensionZ;
        desc.Format = DXGI_FORMAT_R16_UNORM;
        desc.MipLevels = 1;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.Usage = D3D11_USAGE_DEFAULT;

        D3D11_SUBRESOURCE_DATA resourceData = {};
        resourceData.pSysMem = std::data(intensity);
        resourceData.SysMemPitch = sizeof(uint16_t) * desc.Width;
        resourceData.SysMemSlicePitch = sizeof(uint16_t) * desc.Height * desc.Width;

        Microsoft::WRL::ComPtr<ID3D11Texture3D> pTextureIntensity;
        DX::ThrowIfFailed(m_pDevice->CreateTexture3D(&desc, &resourceData, pTextureIntensity.GetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureIntensity.Get(), nullptr, m_pSRVVolumeIntensity.GetAddressOf()));
    }
}

auto ApplicationVolumeRender::InitializeTransferFunction() -> void {
    nlohmann::json root;
    std::ifstream file("Data/TransferFunctions/ManixTransferFunction.json");
    file >> root;

    m_OpacityTransferFunc.Clear();
    m_DiffuseTransferFunc.Clear();
    m_SpecularTransferFunc.Clear();
    m_EmissionTransferFunc.Clear();
    m_RoughnessTransferFunc.Clear();

    auto ExtractVec3FromJson = [](auto const& tree, auto const& key) -> Hawk::Math::Vec3 {
        Hawk::Math::Vec3 v{};
        uint32_t index = 0;
        for (auto& e : tree[key]) {
            v[index] = e.get<float>();
            index++;
        }
        return v;
    };

    for (auto const& e : root["NodesColor"]) {
        auto intensity = e["Intensity"].get<float>();
        auto diffuse = ExtractVec3FromJson(e, "Diffuse");
        auto specular = ExtractVec3FromJson(e, "Specular");
        auto roughness = e["Roughness"].get<float>();

        m_DiffuseTransferFunc.AddNode(intensity, diffuse);
        m_SpecularTransferFunc.AddNode(intensity, specular);
        m_RoughnessTransferFunc.AddNode(intensity, roughness);
    }

    for (auto const& e : root["NodesOpacity"])
        m_OpacityTransferFunc.AddNode(e["Intensity"].get<F32>(), e["Opacity"].get<F32>());

    m_pSRVOpasityTF = m_OpacityTransferFunc.GenerateTexture(m_pDevice, m_SamplingCount);
    m_pSRVDiffuseTF = m_DiffuseTransferFunc.GenerateTexture(m_pDevice, m_SamplingCount);
    m_pSRVSpecularTF = m_SpecularTransferFunc.GenerateTexture(m_pDevice, m_SamplingCount);
    m_pSRVRoughnessTF = m_RoughnessTransferFunc.GenerateTexture(m_pDevice, m_SamplingCount);
}

auto ApplicationVolumeRender::InitializeSamplerStates() -> void {
    auto createSamplerState = [this](auto filter, auto addressMode) -> DX::ComPtr<ID3D11SamplerState> {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter = filter;
        desc.AddressU = addressMode;
        desc.AddressV = addressMode;
        desc.AddressW = addressMode;
        desc.MaxAnisotropy = (m_pDevice->GetFeatureLevel() > D3D_FEATURE_LEVEL_9_1) ? D3D11_MAX_MAXANISOTROPY : 2;
        desc.MaxLOD = FLT_MAX;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;

        Microsoft::WRL::ComPtr<ID3D11SamplerState> pSamplerState;
        DX::ThrowIfFailed(m_pDevice->CreateSamplerState(&desc, pSamplerState.GetAddressOf()));
        return pSamplerState;
    };

    m_pSamplerPoint = createSamplerState(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
    m_pSamplerLinear = createSamplerState(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
    m_pSamplerAnisotropic = createSamplerState(D3D11_FILTER_ANISOTROPIC, D3D11_TEXTURE_ADDRESS_WRAP);
}

auto ApplicationVolumeRender::InitializeRenderTextures() -> void {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureColor;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureColorSum;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureToneMap;

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
        desc.Width = m_ApplicationDesc.Width;
        desc.Height = m_ApplicationDesc.Height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureColor.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureColor.Get(), nullptr, m_pSRVColor.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureColor.Get(), nullptr, m_pUAVColor.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
        desc.Width = m_ApplicationDesc.Width;
        desc.Height = m_ApplicationDesc.Height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureColorSum.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureColorSum.Get(), nullptr, m_pSRVColorSum.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureColorSum.Get(), nullptr, m_pUAVColorSum.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Width = m_ApplicationDesc.Width;
        desc.Height = m_ApplicationDesc.Height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureToneMap.GetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureToneMap.Get(), nullptr, m_pSRVToneMap.GetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureToneMap.Get(), nullptr, m_pUAVToneMap.GetAddressOf()));
    }
}

auto ApplicationVolumeRender::InitializeConstantBuffers() -> void {
    m_pCBFrame = DX::CreateConstantBuffer<FrameBuffer>(m_pDevice);
}

auto ApplicationVolumeRender::InitializeEnviromentMap() -> void {
    DX::ThrowIfFailed(DirectX::CreateDDSTextureFromFile(m_pDevice.Get(), L"Data/Textures/qwantani_2k.dds", nullptr, m_pSRVEnviroment.GetAddressOf()));
}

auto ApplicationVolumeRender::EventMouseWheel(float delta) -> void {
    m_Zoom -= m_ZoomSensivity * m_DeltaTime * delta;
    m_FrameIndex = 0;
}

auto ApplicationVolumeRender::EventMouseMove(float x, float y) -> void {
    m_Camera.Rotate(Hawk::Components::Camera::LocalUp, m_DeltaTime * -m_RotateSensivity * x);
    m_Camera.Rotate(m_Camera.Right(), m_DeltaTime * -m_RotateSensivity * y);
    m_FrameIndex = 0;
}

auto ApplicationVolumeRender::Update(float deltaTime) -> void {
    m_DeltaTime = deltaTime;

    try {
        if (m_IsReloadShader) {
            InitializeShaders();
            m_IsReloadShader = false;
        }

        if (m_IsReloadTranferFunc) {
            InitializeTransferFunction();
            m_IsReloadTranferFunc = false;
        }

    } catch (std::exception const& e) {
        std::cout << e.what() << std::endl;
    }

    Hawk::Math::Vec3 scaleVector = { 0.488f * m_DimensionX, 0.488f * m_DimensionY, 0.7f * m_DimensionZ };
    scaleVector /= (std::max)({ scaleVector.x, scaleVector.y, scaleVector.z });

    auto const& matrixView = m_Camera.ToMatrix();
    Hawk::Math::Mat4x4 matrixProjection = Hawk::Math::Orthographic(m_Zoom * (m_ApplicationDesc.Width / static_cast<F32>(m_ApplicationDesc.Height)), m_Zoom, -2.0f, 2.0f);
    Hawk::Math::Mat4x4 matrixWorld  = Hawk::Math::RotateX(Hawk::Math::Radians(-90.0f));
    Hawk::Math::Mat4x4 matrixNormal = Hawk::Math::Inverse(Hawk::Math::Transpose(matrixWorld));

    {
        DX::MapHelper<FrameBuffer> map(m_pImmediateContext, m_pCBFrame, D3D11_MAP_WRITE_DISCARD, 0);
        map->BoundingBoxMin = scaleVector * m_BoundingBoxMin;
        map->BoundingBoxMax = scaleVector * m_BoundingBoxMax;
        map->ViewProjectionMatrix = matrixProjection * matrixView;
        map->WorldMatrix = matrixWorld;
        map->NormalMatrix = matrixNormal;

        map->InvViewProjectionMatrix = Hawk::Math::Inverse(map->ViewProjectionMatrix);
        map->InvWorldMatrix = Hawk::Math::Inverse(map->WorldMatrix);
        map->InvNormalMatrix = Hawk::Math::Inverse(map->NormalMatrix);

        map->Density = m_Density;
        map->StepCount = m_StepCount;
        map->TraceDepth = m_TraceDepth;
        map->FrameIndex = m_FrameIndex;
        map->Exposure = m_Exposure;

        map->FrameOffset = Hawk::Math::Vec2(2.0f * m_RandomDistribution(m_RandomGenerator) - 1.0f, 2.0f * m_RandomDistribution(m_RandomGenerator) - 1.0f);
        map->DenoiserStrange = m_DenoiserStrange;
        map->RenderTargetDim = Hawk::Math::Vec2(static_cast<F32>(m_ApplicationDesc.Width), static_cast<F32>(m_ApplicationDesc.Height));
    }
}

auto ApplicationVolumeRender::Blit(DX::ComPtr<ID3D11ShaderResourceView> pSrc, DX::ComPtr<ID3D11RenderTargetView> pDst)-> void {
    ID3D11ShaderResourceView* ppSRVClear[] = { nullptr, nullptr, nullptr, nullptr,  nullptr, nullptr, nullptr, nullptr };
    D3D11_VIEWPORT viewport = {0.0f, 0.0f, static_cast<float>(m_ApplicationDesc.Width), static_cast<float>(m_ApplicationDesc.Height), 0.0f, 1.0f};
    
    // Set RTVs
    m_pImmediateContext->OMSetRenderTargets(1, pDst.GetAddressOf(), nullptr);
    m_pImmediateContext->RSSetViewports(1, &viewport);

    // Bind PSO and Resources
    m_PSOBlit.Apply(m_pImmediateContext);
    m_pImmediateContext->PSSetShaderResources(0, 1, pSrc.GetAddressOf());
    m_pImmediateContext->PSSetSamplers(0, 1, m_pSamplerPoint.GetAddressOf());

    // Execute
    m_pImmediateContext->Draw(6, 0);

    // Unbind RTV's
    m_pImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);
    m_pImmediateContext->RSSetViewports(0, nullptr);

    // Unbind PSO and unbind Resources
    m_PSODefault.Apply(m_pImmediateContext);
    m_pImmediateContext->PSSetSamplers(0, 0, nullptr);
    m_pImmediateContext->PSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
}

auto ApplicationVolumeRender::RenderFrame(
    DX::ComPtr<ID3D11RenderTargetView> pRTV) -> void {
    ID3D11UnorderedAccessView* ppUAVClear[] = { nullptr, nullptr, nullptr };
    ID3D11ShaderResourceView* ppSRVClear[] = { nullptr, nullptr, nullptr };

    uint32_t threadGroupsX = static_cast<uint32_t>(std::ceil(m_ApplicationDesc.Width  / static_cast<float>(m_ThreadSizeDimensionX)));
    uint32_t threadGroupsY = static_cast<uint32_t>(std::ceil(m_ApplicationDesc.Height / static_cast<float>(m_ThreadSizeDimensionY)));

    {
        ID3D11SamplerState* ppSamplers[] = {
            m_pSamplerPoint.Get(),
            m_pSamplerLinear.Get(),
            m_pSamplerAnisotropic.Get()
        };

        ID3D11ShaderResourceView* ppSRVTextures[] = {
            m_pSRVVolumeIntensity.Get(),
            m_pSRVDiffuseTF.Get(),
            m_pSRVSpecularTF.Get(),      
            m_pSRVRoughnessTF.Get(),
            m_pSRVOpasityTF.Get(),       
            m_pSRVEnviroment.Get() 
        };

        ID3D11UnorderedAccessView* ppUAVTextures[] = { m_pUAVColor.Get() };

        // First pass Sample Monte Carlo
        m_PSOPathTracing.Apply(m_pImmediateContext);
        m_pImmediateContext->CSSetConstantBuffers(0, 1, m_pCBFrame.GetAddressOf());
        m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplers), ppSamplers);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVTextures), ppSRVTextures);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVTextures), ppUAVTextures, nullptr);
        m_pImmediateContext->Dispatch(threadGroupsX, threadGroupsY, 1);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
    }

    {
        ID3D11ShaderResourceView* ppSRVTextures[] = { m_pSRVColor.Get() };
        ID3D11UnorderedAccessView* ppUAVTextures[] = { m_pUAVColorSum.Get() };

        // Second pass Integrate Monte Carlo
        m_PSOPathTracingSum.Apply(m_pImmediateContext);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVTextures), ppSRVTextures);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVTextures), ppUAVTextures, nullptr);
        m_pImmediateContext->Dispatch(threadGroupsX, threadGroupsY, 1);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
    }

    {
        ID3D11ShaderResourceView* ppSRVTextures[] = { m_pSRVColorSum.Get() };
        ID3D11UnorderedAccessView* ppUAVTextures[] = { m_pUAVToneMap.Get() };

        // Tone map
        m_PSOToneMap.Apply(m_pImmediateContext);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVTextures), ppSRVTextures);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVTextures), ppUAVTextures, nullptr);
        m_pImmediateContext->Dispatch(threadGroupsX, threadGroupsY, 1);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
    }

    this->Blit(m_pSRVToneMap, pRTV);

    m_FrameIndex++;
}

auto ApplicationVolumeRender::RenderGUI(DX::ComPtr<ID3D11RenderTargetView> pRTV) -> void {
    ImGui::Begin("Debug");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    m_FrameIndex = ImGui::Checkbox("Reload shader", &m_IsReloadShader) ? 0 : m_FrameIndex;
    m_FrameIndex = ImGui::Checkbox("Reload tranfer func", &m_IsReloadTranferFunc) ? 0 : m_FrameIndex;
    ImGui::End();

    ImGui::Begin("Settings");
    ImGui::NewLine();
    ImGui::SliderFloat("Rotate sentivity", &m_RotateSensivity, 0.1f, 10.0f);
    ImGui::SliderFloat("Zoom sentivity", &m_ZoomSensivity, 0.1f, 10.0f);
    ImGui::NewLine();
    m_FrameIndex = ImGui::SliderFloat("Density", &m_Density, 0.1f, 100.0f) ? 0 : m_FrameIndex;
    m_FrameIndex = ImGui::SliderInt("Step count", (int32_t*)&m_StepCount, 1, 512) ? 0 : m_FrameIndex;
    m_FrameIndex = ImGui::SliderInt("Trace depth", (int32_t*)&m_TraceDepth, 1, 10) ? 0 : m_FrameIndex;
    m_FrameIndex = ImGui::SliderFloat("Zoom", &m_Zoom, 0.1f, 10.0f) ? 0 : m_FrameIndex;

    ImGui::NewLine();

    ImGui::PlotLines("Opacity", [](void* data, int idx) { return static_cast<ScalarTransferFunction1D*>(data)->Evaluate(idx / static_cast<F32>(256));  }, &m_OpacityTransferFunc, m_SamplingCount);

    ImGui::NewLine();
    ImGui::SliderFloat("Exposure", &m_Exposure, 4.0f, 100.0f);

    ImGui::End();
}
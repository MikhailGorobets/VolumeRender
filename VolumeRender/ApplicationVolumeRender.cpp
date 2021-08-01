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
    Hawk::Math::Mat4x4 NormalViewMatrix;
    Hawk::Math::Mat4x4 WorldViewProjectionMatrix;
    Hawk::Math::Mat4x4 ViewMatrix;
    Hawk::Math::Mat4x4 WorldMatrix;
    Hawk::Math::Mat4x4 NormalMatrix;

    Hawk::Math::Mat4x4 InvViewProjectionMatrix;
    Hawk::Math::Mat4x4 InvNormalViewMatrix;
    Hawk::Math::Mat4x4 InvWorldViewProjectionMatrix;
    Hawk::Math::Mat4x4 InvViewMatrix;
    Hawk::Math::Mat4x4 InvWorldMatrix;
    Hawk::Math::Mat4x4 InvNormalMatrix;

    float Dispersion;
    uint32_t FrameIndex;
    uint32_t TraceDepth;
    uint32_t StepCount;

    float Density;
    Hawk::Math::Vec3 BoundingBoxMin;

    float Exposure;
    Hawk::Math::Vec3 BoundingBoxMax;

    Hawk::Math::Vec2 FrameOffset;
    Hawk::Math::Vec2 RenderTargetDim;
};

struct DispathIndirectBuffer {
    uint32_t ThreadGroupX;
    uint32_t ThreadGroupY;
    uint32_t ThreadGroupZ;
};

struct DrawInstancedIndirectBuffer {
    uint32_t VertexCount;
    uint32_t InstanceCount;
    uint32_t VertexOffset;
    uint32_t InstanceOffset;
};

ApplicationVolumeRender::ApplicationVolumeRender(ApplicationDesc const& desc)
    : Application(desc)
    , m_RandomGenerator(m_RandomDevice())
    , m_RandomDistribution(0.0f, 1.0f) {

    this->InitializeShaders();
    this->InitializeTransferFunction();
    this->InitializeSamplerStates();
    this->InitializeRenderTextures();
    this->InitilizeTileBuffer();
    this->InitializeBuffers();
    this->InitializeVolumeTexture();
    this->InitializeEnviromentMap();
}

auto ApplicationVolumeRender::InitializeShaders() -> void {

    auto compileShader = [](auto fileName, auto entrypoint, auto target, auto macros) -> DX::ComPtr<ID3DBlob> {
        DX::ComPtr<ID3DBlob> pCodeBlob;
        DX::ComPtr<ID3DBlob> pErrorBlob;

        if (FAILED(D3DCompileFromFile(fileName, macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, entrypoint, target, 0, 0, pCodeBlob.GetAddressOf(), pErrorBlob.GetAddressOf())))
            throw std::runtime_error(static_cast<const char*>(pErrorBlob->GetBufferPointer()));
        return pCodeBlob;
    };

    std::string threadSizeX = std::to_string(8);
    std::string threadSizeY = std::to_string(8);

    D3D_SHADER_MACRO macros[] = {
        {"THREAD_GROUP_SIZE_X", threadSizeX.c_str()},
        {"THREAD_GROUP_SIZE_Y", threadSizeY.c_str()},
        { nullptr, nullptr}
    };

    auto pBlobCSRayTrace = compileShader(L"Data/Shaders/RayTracing.hlsl", "RayTrace", "cs_5_0", macros);
    auto pBlobCSAccumulate = compileShader(L"Data/Shaders/Accumulation.hlsl", "Accumulate", "cs_5_0", macros);
    auto pBlobCSDispersion = compileShader(L"Data/Shaders/Dispersion.hlsl", "Dispersion", "cs_5_0", macros);
    auto pBlobCSToneMap = compileShader(L"Data/Shaders/ToneMap.hlsl", "ToneMap", "cs_5_0", macros);
    auto pBlobCSComputeGradient = compileShader(L"Data/Shaders/Gradient.hlsl", "ComputeGradient", "cs_5_0", macros);
    auto pBlobCSGenerateMipLevel = compileShader(L"Data/Shaders/LevelOfDetail.hlsl", "GenerateMipLevel", "cs_5_0", macros);
    auto pBlobCSResetTiles = compileShader(L"Data/Shaders/ResetTiles.hlsl", "ResetTiles", "cs_5_0", macros);
    auto pBlobVSBlit = compileShader(L"Data/Shaders/Blitting.hlsl", "BlitVS", "vs_5_0", macros);
    auto pBlobPSBlit = compileShader(L"Data/Shaders/Blitting.hlsl", "BlitPS", "ps_5_0", macros);
    auto pBlobVSDebugTiles = compileShader(L"Data/Shaders/Debug.hlsl", "DebugTilesVS", "vs_5_0", macros);
    auto pBlobGSDebugTiles = compileShader(L"Data/Shaders/Debug.hlsl", "DebugTilesGS", "gs_5_0", macros);
    auto pBlobPSDegugTiles = compileShader(L"Data/Shaders/Debug.hlsl", "DebugTilesPS", "ps_5_0", macros);


    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSRayTrace->GetBufferPointer(), pBlobCSRayTrace->GetBufferSize(), nullptr, m_PSORayTrace.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSAccumulate->GetBufferPointer(), pBlobCSAccumulate->GetBufferSize(), nullptr, m_PSOAccumulate.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSDispersion->GetBufferPointer(), pBlobCSDispersion->GetBufferSize(), nullptr, m_PSODispersion.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSToneMap->GetBufferPointer(), pBlobCSToneMap->GetBufferSize(), nullptr, m_PSOToneMap.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSGenerateMipLevel->GetBufferPointer(), pBlobCSGenerateMipLevel->GetBufferSize(), nullptr, m_PSOGenerateMipLevel.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSComputeGradient->GetBufferPointer(), pBlobCSComputeGradient->GetBufferSize(), nullptr, m_PSOComputeGradient.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSResetTiles->GetBufferPointer(), pBlobCSResetTiles->GetBufferSize(), nullptr, m_PSOResetTiles.pCS.ReleaseAndGetAddressOf()));

    DX::ThrowIfFailed(m_pDevice->CreateVertexShader(pBlobVSBlit->GetBufferPointer(), pBlobVSBlit->GetBufferSize(), nullptr, m_PSOBlit.pVS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreatePixelShader(pBlobPSBlit->GetBufferPointer(), pBlobPSBlit->GetBufferSize(), nullptr, m_PSOBlit.pPS.ReleaseAndGetAddressOf()));
    m_PSOBlit.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    DX::ThrowIfFailed(m_pDevice->CreateVertexShader(pBlobVSDebugTiles->GetBufferPointer(), pBlobVSDebugTiles->GetBufferSize(), nullptr, m_PSODegugTiles.pVS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateGeometryShader(pBlobGSDebugTiles->GetBufferPointer(), pBlobGSDebugTiles->GetBufferSize(), nullptr, m_PSODegugTiles.pGS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreatePixelShader(pBlobPSDegugTiles->GetBufferPointer(), pBlobPSDegugTiles->GetBufferSize(), nullptr, m_PSODegugTiles.pPS.ReleaseAndGetAddressOf()));
    m_PSODegugTiles.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;

}

auto ApplicationVolumeRender::InitializeVolumeTexture() -> void {
    std::ifstream file("Data/Textures/manix.dat", std::ifstream::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open file: " + std::string("Data/Textures/manix.dat"));

    file.read(reinterpret_cast<char*>(&m_DimensionX), sizeof(uint16_t));
    file.read(reinterpret_cast<char*>(&m_DimensionY), sizeof(uint16_t));
    file.read(reinterpret_cast<char*>(&m_DimensionZ), sizeof(uint16_t));
    m_DimensionMipLevels = static_cast<uint16_t>(std::ceil(std::log2(std::max(std::max(m_DimensionX, m_DimensionY), m_DimensionZ)))) + 1;

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
        DX::ComPtr<ID3D11Texture3D> pTextureIntensity;
        D3D11_TEXTURE3D_DESC desc = {};
        desc.Width = m_DimensionX;
        desc.Height = m_DimensionY;
        desc.Depth = m_DimensionZ;
        desc.Format = DXGI_FORMAT_R16_UNORM;
        desc.MipLevels = m_DimensionMipLevels;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.Usage = D3D11_USAGE_DEFAULT;;
        DX::ThrowIfFailed(m_pDevice->CreateTexture3D(&desc, nullptr, pTextureIntensity.GetAddressOf()));

        for (uint32_t mipLevelID = 0; mipLevelID < desc.MipLevels; mipLevelID++) {
            D3D11_SHADER_RESOURCE_VIEW_DESC descSRV = {};
            descSRV.Format = DXGI_FORMAT_R16_UNORM;
            descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
            descSRV.Texture3D.MipLevels = 1;
            descSRV.Texture3D.MostDetailedMip = mipLevelID;

            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pSRVVolumeIntensity;
            DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureIntensity.Get(), &descSRV, pSRVVolumeIntensity.GetAddressOf()));
            m_pSRVVolumeIntensity.push_back(pSRVVolumeIntensity);
        }

        for (uint32_t mipLevelID = 0; mipLevelID < desc.MipLevels; mipLevelID++) {
            D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV = {};
            descUAV.Format = DXGI_FORMAT_R16_UNORM;
            descUAV.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
            descUAV.Texture3D.MipSlice = mipLevelID;
            descUAV.Texture3D.FirstWSlice = 0;
            descUAV.Texture3D.WSize = std::max(m_DimensionZ >> mipLevelID, 1);

            Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> pUAVVolumeIntensity;
            DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureIntensity.Get(), &descUAV, pUAVVolumeIntensity.GetAddressOf()));
            m_pUAVVolumeIntensity.push_back(pUAVVolumeIntensity);
        }

        D3D11_BOX box = {0, 0, 0,  desc.Width, desc.Height,  desc.Depth};
        m_pImmediateContext->UpdateSubresource(pTextureIntensity.Get(), 0, &box, std::data(intensity), sizeof(uint16_t) * desc.Width, sizeof(uint16_t) * desc.Height * desc.Width);

        for (uint32_t mipLevelID = 1; mipLevelID < desc.MipLevels - 1; mipLevelID++) {
            uint32_t threadGroupX = std::max(static_cast<uint32_t>(std::ceil((m_DimensionX >> mipLevelID) / 4.0f)), 1u);
            uint32_t threadGroupY = std::max(static_cast<uint32_t>(std::ceil((m_DimensionY >> mipLevelID) / 4.0f)), 1u);
            uint32_t threadGroupZ = std::max(static_cast<uint32_t>(std::ceil((m_DimensionZ >> mipLevelID) / 4.0f)), 1u);

            ID3D11ShaderResourceView* ppSRVTextures[] = {m_pSRVVolumeIntensity[mipLevelID - 1].Get()};
            ID3D11UnorderedAccessView* ppUAVTextures[] = {m_pUAVVolumeIntensity[mipLevelID + 0].Get()};
            ID3D11SamplerState* ppSamplers[] = {m_pSamplerLinear.Get()};

            ID3D11UnorderedAccessView* ppUAVClear[] = {nullptr};
            ID3D11ShaderResourceView* ppSRVClear[] = {nullptr};
            ID3D11SamplerState* ppSamplerClear[] = {nullptr};

            m_PSOGenerateMipLevel.Apply(m_pImmediateContext);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVTextures), ppSRVTextures);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVTextures), ppUAVTextures, nullptr);
            m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplers), ppSamplers);
            m_pImmediateContext->Dispatch(threadGroupX, threadGroupY, threadGroupZ);

            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
            m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplerClear), ppSamplerClear);
        }
        m_pImmediateContext->Flush();
    }

    {
        DX::ComPtr<ID3D11Texture3D> pTextureGradient;
        D3D11_TEXTURE3D_DESC desc = {};
        desc.Width = m_DimensionX;
        desc.Height = m_DimensionY;
        desc.Depth = m_DimensionZ;
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.MipLevels = 1;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.Usage = D3D11_USAGE_DEFAULT;
        DX::ThrowIfFailed(m_pDevice->CreateTexture3D(&desc, nullptr, pTextureGradient.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureGradient.Get(), nullptr, m_pSRVGradient.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureGradient.Get(), nullptr, m_pUAVGradient.ReleaseAndGetAddressOf()));
        {
            uint32_t threadGroupX = static_cast<uint32_t>(std::ceil(m_DimensionX / 4.0f));
            uint32_t threadGroupY = static_cast<uint32_t>(std::ceil(m_DimensionY / 4.0f));
            uint32_t threadGroupZ = static_cast<uint32_t>(std::ceil(m_DimensionZ / 4.0f));

            ID3D11ShaderResourceView* ppSRVTextures[] = {m_pSRVVolumeIntensity[0].Get(), m_pSRVOpasityTF.Get()};
            ID3D11UnorderedAccessView* ppUAVTextures[] = {m_pUAVGradient.Get()};
            ID3D11SamplerState* ppSamplers[] = {m_pSamplerPoint.Get(), m_pSamplerLinear.Get()};

            ID3D11UnorderedAccessView* ppUAVClear[] = {nullptr};
            ID3D11ShaderResourceView* ppSRVClear[] = {nullptr, nullptr};
            ID3D11SamplerState* ppSamplerClear[] = {nullptr, nullptr};

            m_PSOComputeGradient.Apply(m_pImmediateContext);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVTextures), ppSRVTextures);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVTextures), ppUAVTextures, nullptr);
            m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplers), ppSamplers);
            m_pImmediateContext->Dispatch(threadGroupX, threadGroupY, threadGroupZ);

            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
            m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplerClear), ppSamplerClear);
        }
        m_pImmediateContext->Flush();
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
        Hawk::Math::Vec4 color = {0.0f, 0.0f, 0.0f, 0.0f};

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

    m_pSamplerPoint = createSamplerState(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_BORDER);
    m_pSamplerLinear = createSamplerState(D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D11_TEXTURE_ADDRESS_BORDER);
    m_pSamplerAnisotropic = createSamplerState(D3D11_FILTER_ANISOTROPIC, D3D11_TEXTURE_ADDRESS_WRAP);
}

auto ApplicationVolumeRender::InitializeRenderTextures() -> void {

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

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureColor;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureColor.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureColor.Get(), nullptr, m_pSRVColor.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureColor.Get(), nullptr, m_pUAVColor.ReleaseAndGetAddressOf()));
    }

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

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureNormal;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureNormal.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureNormal.Get(), nullptr, m_pSRVNormal.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureNormal.Get(), nullptr, m_pUAVNormal.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R32_FLOAT;
        desc.Width = m_ApplicationDesc.Width;
        desc.Height = m_ApplicationDesc.Height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureDepth;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureDepth.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureDepth.Get(), nullptr, m_pSRVDepth.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureDepth.Get(), nullptr, m_pUAVDepth.ReleaseAndGetAddressOf()));
    }


    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        desc.Width = m_ApplicationDesc.Width;
        desc.Height = m_ApplicationDesc.Height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureColorSum;
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

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureToneMap;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureToneMap.GetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureToneMap.Get(), nullptr, m_pSRVToneMap.GetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureToneMap.Get(), nullptr, m_pUAVToneMap.GetAddressOf()));
    }

}

auto ApplicationVolumeRender::InitializeBuffers() -> void {

    m_pConstantBufferFrame = DX::CreateConstantBuffer<FrameBuffer>(m_pDevice);
    {
        DispathIndirectBuffer arguments = {1, 1, 1};
        m_pDispathIndirectBufferArgs = DX::CreateIndirectBuffer<DispathIndirectBuffer>(m_pDevice, &arguments);
    }

    {
        DrawInstancedIndirectBuffer arguments = {0, 1, 0, 0};
        m_pDrawInstancedIndirectBufferArgs = DX::CreateIndirectBuffer<DrawInstancedIndirectBuffer>(m_pDevice, &arguments);
    }
}

auto ApplicationVolumeRender::InitilizeTileBuffer() -> void {

    uint32_t threadGroupsX = static_cast<uint32_t>(std::ceil(m_ApplicationDesc.Width / 8));
    uint32_t threadGroupsY = static_cast<uint32_t>(std::ceil(m_ApplicationDesc.Height / 8));

    DX::ComPtr<ID3D11Buffer> pBuffer = DX::CreateStructuredBuffer<uint32_t>(m_pDevice, threadGroupsX * threadGroupsY, false, true, nullptr);
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        desc.BufferEx.FirstElement = 0;
        desc.BufferEx.NumElements = threadGroupsX * threadGroupsY;
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pBuffer.Get(), &desc, m_pSRVDispersionTiles.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {};
        desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        desc.Buffer.FirstElement = 0;
        desc.Buffer.NumElements = threadGroupsX * threadGroupsY;
        desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pBuffer.Get(), &desc, m_pUAVDispersionTiles.ReleaseAndGetAddressOf()));
    }
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

    Hawk::Math::Vec3 scaleVector = {0.488f * m_DimensionX, 0.488f * m_DimensionY, 0.7f * m_DimensionZ};
    scaleVector /= (std::max)({scaleVector.x, scaleVector.y, scaleVector.z});

    auto const& matrixView = m_Camera.ToMatrix();
    Hawk::Math::Mat4x4 matrixProjection = Hawk::Math::Orthographic(m_Zoom * (m_ApplicationDesc.Width / static_cast<F32>(m_ApplicationDesc.Height)), m_Zoom, -2.0f, 2.0f);
    Hawk::Math::Mat4x4 matrixWorld = Hawk::Math::RotateX(Hawk::Math::Radians(-90.0f));
    Hawk::Math::Mat4x4 matrixNormal = Hawk::Math::Inverse(Hawk::Math::Transpose(matrixWorld));

    {
        DX::MapHelper<FrameBuffer> map(m_pImmediateContext, m_pConstantBufferFrame, D3D11_MAP_WRITE_DISCARD, 0);
        map->BoundingBoxMin = scaleVector * m_BoundingBoxMin;
        map->BoundingBoxMax = scaleVector * m_BoundingBoxMax;

        map->ViewProjectionMatrix = matrixProjection * matrixView;
        map->NormalViewMatrix = matrixView * matrixNormal;
        map->WorldViewProjectionMatrix = matrixProjection * matrixView * matrixWorld;
        map->ViewMatrix = matrixView;
        map->WorldMatrix = matrixWorld;
        map->NormalMatrix = matrixNormal;

        map->InvViewProjectionMatrix = Hawk::Math::Inverse(map->ViewProjectionMatrix);
        map->InvNormalViewMatrix = Hawk::Math::Inverse(map->NormalViewMatrix);
        map->InvWorldViewProjectionMatrix = Hawk::Math::Inverse(map->WorldViewProjectionMatrix);
        map->InvViewMatrix = Hawk::Math::Inverse(map->InvViewMatrix);
        map->InvWorldMatrix = Hawk::Math::Inverse(map->WorldMatrix);
        map->InvNormalMatrix = Hawk::Math::Inverse(map->NormalMatrix);

        map->Density = m_Density;
        map->StepCount = m_StepCount;
        map->TraceDepth = m_TraceDepth;
        map->FrameIndex = m_FrameIndex;
        map->Exposure = m_Exposure;
        map->Dispersion = m_Dispersion;

        map->FrameOffset = Hawk::Math::Vec2(m_RandomDistribution(m_RandomGenerator), m_RandomDistribution(m_RandomGenerator));
        map->RenderTargetDim = Hawk::Math::Vec2(static_cast<F32>(m_ApplicationDesc.Width), static_cast<F32>(m_ApplicationDesc.Height));
    }
}

auto ApplicationVolumeRender::Blit(DX::ComPtr<ID3D11ShaderResourceView> pSrc, DX::ComPtr<ID3D11RenderTargetView> pDst)-> void {
    ID3D11ShaderResourceView* ppSRVClear[] = {nullptr, nullptr, nullptr, nullptr,  nullptr, nullptr, nullptr, nullptr};
    D3D11_VIEWPORT viewport = {0.0f, 0.0f, static_cast<float>(m_ApplicationDesc.Width), static_cast<float>(m_ApplicationDesc.Height), 0.0f, 1.0f};

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

auto ApplicationVolumeRender::RenderFrame(DX::ComPtr<ID3D11RenderTargetView> pRTV) -> void {

    ID3D11UnorderedAccessView* ppUAVClear[] = {nullptr, nullptr, nullptr};
    ID3D11ShaderResourceView* ppSRVClear[] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

    uint32_t threadGroupsX = static_cast<uint32_t>(std::ceil(m_ApplicationDesc.Width / 8));
    uint32_t threadGroupsY = static_cast<uint32_t>(std::ceil(m_ApplicationDesc.Height / 8));

    m_pImmediateContext->VSSetConstantBuffers(0, 1, m_pConstantBufferFrame.GetAddressOf());
    m_pImmediateContext->GSSetConstantBuffers(0, 1, m_pConstantBufferFrame.GetAddressOf());
    m_pImmediateContext->PSSetConstantBuffers(0, 1, m_pConstantBufferFrame.GetAddressOf());
    m_pImmediateContext->CSSetConstantBuffers(0, 1, m_pConstantBufferFrame.GetAddressOf());

    if (m_FrameIndex < m_SampleDispersion) {
        ID3D11UnorderedAccessView* ppUAVResources[] = {m_pUAVDispersionTiles.Get()};
        uint32_t pCounters[] = {0};

        m_pAnnotation->BeginEvent(L"Render Pass: Reset computed tiles");
        m_PSOResetTiles.Apply(m_pImmediateContext);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVResources), ppUAVResources, pCounters);
        m_pImmediateContext->Dispatch(threadGroupsX, threadGroupsY, 1);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
        m_pAnnotation->EndEvent();
    } else {
        ID3D11ShaderResourceView* ppSRVResources[] = {m_pSRVToneMap.Get(), m_pSRVNormal.Get(), m_pSRVDepth.Get()};
        ID3D11UnorderedAccessView* ppUAVResources[] = {m_pUAVDispersionTiles.Get()};
        uint32_t pCounters[] = {0};

        m_pAnnotation->BeginEvent(L"Render Pass: Generete computed tiles");
        m_PSODispersion.Apply(m_pImmediateContext);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVResources), ppSRVResources);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVResources), ppUAVResources, pCounters);
        m_pImmediateContext->Dispatch(threadGroupsX, threadGroupsY, 1);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
        m_pAnnotation->EndEvent();
    }

    float clearColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
    m_pAnnotation->BeginEvent(L"Render Pass: Clear buffers [Color, Normal, Depth]");
    m_pImmediateContext->ClearUnorderedAccessViewFloat(m_pUAVColor.Get(), clearColor);
    m_pImmediateContext->ClearUnorderedAccessViewFloat(m_pUAVNormal.Get(), clearColor);
    m_pImmediateContext->ClearUnorderedAccessViewFloat(m_pUAVDepth.Get(), clearColor);
    m_pAnnotation->EndEvent();

    m_pAnnotation->BeginEvent(L"Render Pass: Copy counters of tiles");
    m_pImmediateContext->CopyStructureCount(m_pDispathIndirectBufferArgs.Get(), 0, m_pUAVDispersionTiles.Get());
    m_pImmediateContext->CopyStructureCount(m_pDrawInstancedIndirectBufferArgs.Get(), 0, m_pUAVDispersionTiles.Get());
    m_pAnnotation->EndEvent();

    {
        ID3D11SamplerState* ppSamplers[] = {
            m_pSamplerPoint.Get(),
            m_pSamplerLinear.Get(),
            m_pSamplerAnisotropic.Get()
        };

        ID3D11ShaderResourceView* ppSRVResources[] = {
            m_pSRVVolumeIntensity[m_MipLevel].Get(),
            m_pSRVGradient.Get(),
            m_pSRVDiffuseTF.Get(),
            m_pSRVSpecularTF.Get(),
            m_pSRVRoughnessTF.Get(),
            m_pSRVOpasityTF.Get(),
            m_pSRVEnviroment.Get(),
            m_pSRVDispersionTiles.Get()
        };

        ID3D11UnorderedAccessView* ppUAVResources[] = {
            m_pUAVColor.Get(),
            m_pUAVNormal.Get(),
            m_pUAVDepth.Get()
        };

        m_pAnnotation->BeginEvent(L"Render pass: Ray trace");
        m_PSORayTrace.Apply(m_pImmediateContext);
        m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplers), ppSamplers);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVResources), ppSRVResources);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVResources), ppUAVResources, nullptr);
        m_pImmediateContext->DispatchIndirect(m_pDispathIndirectBufferArgs.Get(), 0);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
        m_pAnnotation->EndEvent();
    }

    {
        ID3D11ShaderResourceView* ppSRVResources[] = {m_pSRVColor.Get(),  m_pSRVDispersionTiles.Get()};
        ID3D11UnorderedAccessView* ppUAVResources[] = {m_pUAVColorSum.Get()};

        m_pAnnotation->BeginEvent(L"Render Pass: Accumulate");
        m_PSOAccumulate.Apply(m_pImmediateContext);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVResources), ppSRVResources);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVResources), ppUAVResources, nullptr);
        m_pImmediateContext->DispatchIndirect(m_pDispathIndirectBufferArgs.Get(), 0);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
        m_pAnnotation->EndEvent();
    }

    {
        ID3D11ShaderResourceView* ppSRVResources[] = {m_pSRVColorSum.Get(), m_pSRVDispersionTiles.Get()};
        ID3D11UnorderedAccessView* ppUAVResources[] = {m_pUAVToneMap.Get()};

        m_pAnnotation->BeginEvent(L"Render Pass: Tone Map");
        m_PSOToneMap.Apply(m_pImmediateContext);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVResources), ppSRVResources);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVResources), ppUAVResources, nullptr);
        m_pImmediateContext->DispatchIndirect(m_pDispathIndirectBufferArgs.Get(), 0);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
        m_pAnnotation->EndEvent();
    }

    m_pAnnotation->BeginEvent(L"Render Pass: Blit [Tone Map] -> [Back Buffer]");
    this->Blit(m_pSRVToneMap, pRTV);
    m_pAnnotation->EndEvent();

    if (m_IsDrawDegugTiles) {
        ID3D11ShaderResourceView* ppSRVResources[] = {m_pSRVDispersionTiles.Get()};
        D3D11_VIEWPORT viewport = {0.0f, 0.0f, static_cast<float>(m_ApplicationDesc.Width), static_cast<float>(m_ApplicationDesc.Height), 0.0f, 1.0f};

        m_pAnnotation->BeginEvent(L"Render Pass: Debug -> [Generated tiles]");
        m_pImmediateContext->OMSetRenderTargets(1, pRTV.GetAddressOf(), nullptr);
        m_pImmediateContext->RSSetViewports(1, &viewport);

        m_PSODegugTiles.Apply(m_pImmediateContext);
        m_pImmediateContext->VSSetShaderResources(0, _countof(ppSRVResources), ppSRVResources);
        m_pImmediateContext->DrawInstancedIndirect(m_pDrawInstancedIndirectBufferArgs.Get(), 0);

        m_pImmediateContext->VSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
        m_PSODefault.Apply(m_pImmediateContext);
        m_pImmediateContext->RSSetViewports(0, nullptr);
        m_pImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);
        m_pAnnotation->EndEvent();
    }
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

    ImGui::SliderFloat("Density", &m_Density, 0.1f, 100.0f);
    ImGui::SliderInt("Step count", (int32_t*)&m_StepCount, 1, 512);
    ImGui::SliderInt("Trace depth", (int32_t*)&m_TraceDepth, 1, 10);
    m_FrameIndex = ImGui::SliderInt("Mip Level", (int32_t*)&m_MipLevel, 0, m_DimensionMipLevels - 1) ? 0 : m_FrameIndex;
    m_FrameIndex = ImGui::SliderFloat("Dispersion", &m_Dispersion, 0.0f, 0.1f, "%.4f") ? 0 : m_FrameIndex;

    ImGui::NewLine();

    ImGui::PlotLines("Opacity", [](void* data, int idx) { return static_cast<ScalarTransferFunction1D*>(data)->Evaluate(idx / static_cast<F32>(256));  }, &m_OpacityTransferFunc, m_SamplingCount);

    ImGui::NewLine();
    ImGui::SliderFloat("Exposure", &m_Exposure, 4.0f, 100.0f);
    ImGui::NewLine();
    ImGui::Checkbox("DEBUG: Draw tiles", &m_IsDrawDegugTiles);

    ImGui::End();
}
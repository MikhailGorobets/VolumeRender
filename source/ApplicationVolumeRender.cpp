/*
 * MIT License
 *
 * Copyright(c) 2021-2023 Mikhail Gorobets
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
#include <directx-tex/DDSTextureLoader.h>
#include <imgui/imgui.h>
#include <implot/implot.h>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <d3dcompiler.h>
#include <fstream>
#include <iostream>
#include <random>

struct FrameBuffer {

    Hawk::Math::Mat4x4 ProjectionMatrix;
    Hawk::Math::Mat4x4 ViewMatrix;
    Hawk::Math::Mat4x4 WorldMatrix;
    Hawk::Math::Mat4x4 NormalMatrix;

    Hawk::Math::Mat4x4 InvProjectionMatrix;
    Hawk::Math::Mat4x4 InvViewMatrix;
    Hawk::Math::Mat4x4 InvWorldMatrix;
    Hawk::Math::Mat4x4 InvNormalMatrix;

    Hawk::Math::Mat4x4 ViewProjectionMatrix;
    Hawk::Math::Mat4x4 NormalViewMatrix;
    Hawk::Math::Mat4x4 WorldViewProjectionMatrix;

    Hawk::Math::Mat4x4 InvViewProjectionMatrix;
    Hawk::Math::Mat4x4 InvNormalViewMatrix;
    Hawk::Math::Mat4x4 InvWorldViewProjectionMatrix;

    uint32_t         FrameIndex;
    float            StepSize;
    Hawk::Math::Vec2 FrameOffset;

    Hawk::Math::Vec2 InvRenderTargetDim;
    Hawk::Math::Vec2 RenderTargetDim;

    float Density;
    Hawk::Math::Vec3 BoundingBoxMin;

    float Exposure;
    Hawk::Math::Vec3 BoundingBoxMax;
};

struct DispatchIndirectBuffer {
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
    , m_RandomDistribution(-0.5f, +0.5f) {

    this->InitializeShaders();
    this->InitializeTransferFunction();
    this->InitializeSamplerStates();
    this->InitializeRenderTextures();
    this->InitializeTileBuffer();
    this->InitializeBuffers();
    this->InitializeVolumeTexture();
    this->InitializeEnvironmentMap();
}

void ApplicationVolumeRender::InitializeShaders() {

    auto compileShader = [](auto fileName, auto entryPoint, auto target, auto macros) -> DX::ComPtr<ID3DBlob> {
        DX::ComPtr<ID3DBlob> pCodeBlob;
        DX::ComPtr<ID3DBlob> pErrorBlob;

        uint32_t flags = 0;
#if defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG;
#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        if (FAILED(D3DCompileFromFile(fileName, macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, target, flags, 0, pCodeBlob.GetAddressOf(), pErrorBlob.GetAddressOf())))
            throw std::runtime_error(static_cast<const char*>(pErrorBlob->GetBufferPointer()));
        return pCodeBlob;
    };

    //TODO AMD 8x8x1 NV 8x4x1
    const auto threadSizeX = std::to_string(8);
    const auto threadSizeY = std::to_string(8);

    D3D_SHADER_MACRO macros[] = {
        {"THREAD_GROUP_SIZE_X", threadSizeX.c_str()},
        {"THREAD_GROUP_SIZE_Y", threadSizeY.c_str()},
        { nullptr, nullptr}
    };

    auto pBlobCSGeneratePrimaryRays = compileShader(L"content/Shaders/ComputePrimaryRays.hlsl", "GenerateRays", "cs_5_0", macros);
    auto pBlobCSComputeDiffuseLight = compileShader(L"content/Shaders/ComputeRadiance.hlsl", "ComputeRadiance", "cs_5_0", macros);
    auto pBlobCSAccumulate = compileShader(L"content/Shaders/Accumulation.hlsl", "Accumulate", "cs_5_0", macros);
    auto pBlobCSComputeTiles = compileShader(L"content/Shaders/ComputeTiles.hlsl", "ComputeTiles", "cs_5_0", macros);
    auto pBlobCSToneMap = compileShader(L"content/Shaders/ToneMap.hlsl", "ToneMap", "cs_5_0", macros);
    auto pBlobCSComputeGradient = compileShader(L"content/Shaders/ComputeGradient.hlsl", "ComputeGradient", "cs_5_0", macros);
    auto pBlobCSGenerateMipLevel = compileShader(L"content/Shaders/ComputeLevelOfDetail.hlsl", "GenerateMipLevel", "cs_5_0", macros);
    auto pBlobCSResetTiles = compileShader(L"content/Shaders/ComputeTiles.hlsl", "ResetTiles", "cs_5_0", macros);
    auto pBlobVSTextureBlit = compileShader(L"content/Shaders/TextureBlit.hlsl", "BlitVS", "vs_5_0", macros);
    auto pBlobPSTextureBlit = compileShader(L"content/Shaders/TextureBlit.hlsl", "BlitPS", "ps_5_0", macros);
    auto pBlobVSDebugTiles = compileShader(L"content/Shaders/DebugTiles.hlsl", "DebugTilesVS", "vs_5_0", macros);
    auto pBlobGSDebugTiles = compileShader(L"content/Shaders/DebugTiles.hlsl", "DebugTilesGS", "gs_5_0", macros);
    auto pBlobPSDegugTiles = compileShader(L"content/Shaders/DebugTiles.hlsl", "DebugTilesPS", "ps_5_0", macros);


    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSGeneratePrimaryRays->GetBufferPointer(), pBlobCSGeneratePrimaryRays->GetBufferSize(), nullptr, m_PSOGeneratePrimaryRays.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSComputeDiffuseLight->GetBufferPointer(), pBlobCSComputeDiffuseLight->GetBufferSize(), nullptr, m_PSOComputeDiffuseLight.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSAccumulate->GetBufferPointer(), pBlobCSAccumulate->GetBufferSize(), nullptr, m_PSOAccumulate.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSComputeTiles->GetBufferPointer(), pBlobCSComputeTiles->GetBufferSize(), nullptr, m_PSOComputeTiles.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSToneMap->GetBufferPointer(), pBlobCSToneMap->GetBufferSize(), nullptr, m_PSOToneMap.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSGenerateMipLevel->GetBufferPointer(), pBlobCSGenerateMipLevel->GetBufferSize(), nullptr, m_PSOGenerateMipLevel.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSComputeGradient->GetBufferPointer(), pBlobCSComputeGradient->GetBufferSize(), nullptr, m_PSOComputeGradient.pCS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSResetTiles->GetBufferPointer(), pBlobCSResetTiles->GetBufferSize(), nullptr, m_PSOResetTiles.pCS.ReleaseAndGetAddressOf()));

    DX::ThrowIfFailed(m_pDevice->CreateVertexShader(pBlobVSTextureBlit->GetBufferPointer(), pBlobVSTextureBlit->GetBufferSize(), nullptr, m_PSOBlit.pVS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreatePixelShader(pBlobPSTextureBlit->GetBufferPointer(), pBlobPSTextureBlit->GetBufferSize(), nullptr, m_PSOBlit.pPS.ReleaseAndGetAddressOf()));
    m_PSOBlit.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    DX::ThrowIfFailed(m_pDevice->CreateVertexShader(pBlobVSDebugTiles->GetBufferPointer(), pBlobVSDebugTiles->GetBufferSize(), nullptr, m_PSODegugTiles.pVS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreateGeometryShader(pBlobGSDebugTiles->GetBufferPointer(), pBlobGSDebugTiles->GetBufferSize(), nullptr, m_PSODegugTiles.pGS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(m_pDevice->CreatePixelShader(pBlobPSDegugTiles->GetBufferPointer(), pBlobPSDegugTiles->GetBufferSize(), nullptr, m_PSODegugTiles.pPS.ReleaseAndGetAddressOf()));
    m_PSODegugTiles.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
}

void ApplicationVolumeRender::InitializeVolumeTexture() {

    std::unique_ptr<FILE, decltype(&fclose)> pFile(fopen("content/Textures/manix.dat", "rb"), fclose);
    if (!pFile)
        throw std::runtime_error("Failed to open file: " + std::string("Data/Textures/manix.dat"));

    fread(&m_DimensionX, sizeof(uint16_t), 1, pFile.get());
    fread(&m_DimensionY, sizeof(uint16_t), 1, pFile.get());
    fread(&m_DimensionZ, sizeof(uint16_t), 1, pFile.get());

    std::vector<uint16_t> intensity(size_t(m_DimensionX) * size_t(m_DimensionY) * size_t(m_DimensionZ));
    fread(intensity.data(), sizeof(uint16_t), m_DimensionX * m_DimensionY * m_DimensionZ, pFile.get());
    m_DimensionMipLevels = static_cast<uint16_t>(std::ceil(std::log2(std::max(std::max(m_DimensionX, m_DimensionY), m_DimensionZ)))) + 1;

    auto NormalizeIntensity = [](uint16_t intensity, uint16_t min, uint16_t max) -> uint16_t {
        return static_cast<uint16_t>(std::round(std::numeric_limits<uint16_t>::max() * ((intensity - min) / static_cast<F32>(max - min))));
    };

    uint16_t tmin = 0 << 12; // Min HU [0, 4096]
    uint16_t tmax = 1 << 12; // Max HU [0, 4096]
    for (size_t index = 0u; index < std::size(intensity); index++)
        intensity[index] = NormalizeIntensity(intensity[index], tmin, tmax);

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

            DX::ComPtr<ID3D11ShaderResourceView> pSRVVolumeIntensity;
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

            DX::ComPtr<ID3D11UnorderedAccessView> pUAVVolumeIntensity;
            DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureIntensity.Get(), &descUAV, pUAVVolumeIntensity.GetAddressOf()));
            m_pUAVVolumeIntensity.push_back(pUAVVolumeIntensity);
        }

        D3D11_BOX box = { 0, 0, 0,  desc.Width, desc.Height,  desc.Depth };
        m_pImmediateContext->UpdateSubresource(pTextureIntensity.Get(), 0, &box, std::data(intensity), sizeof(uint16_t) * desc.Width, sizeof(uint16_t) * desc.Height * desc.Width);

        for (uint32_t mipLevelID = 1; mipLevelID < desc.MipLevels - 1; mipLevelID++) {
            uint32_t threadGroupX = std::max(static_cast<uint32_t>(std::ceil((m_DimensionX >> mipLevelID) / 4.0f)), 1u);
            uint32_t threadGroupY = std::max(static_cast<uint32_t>(std::ceil((m_DimensionY >> mipLevelID) / 4.0f)), 1u);
            uint32_t threadGroupZ = std::max(static_cast<uint32_t>(std::ceil((m_DimensionZ >> mipLevelID) / 4.0f)), 1u);

            ID3D11ShaderResourceView* ppSRVTextures[] = { m_pSRVVolumeIntensity[mipLevelID - 1].Get() };
            ID3D11UnorderedAccessView* ppUAVTextures[] = { m_pUAVVolumeIntensity[mipLevelID + 0].Get() };
            ID3D11SamplerState* ppSamplers[] = { m_pSamplerLinear.Get() };

            ID3D11UnorderedAccessView* ppUAVClear[] = { nullptr };
            ID3D11ShaderResourceView* ppSRVClear[] = { nullptr };
            ID3D11SamplerState* ppSamplerClear[] = { nullptr };

            auto renderPassName = std::format(L"Render Pass: Compute Mip Map [{}] ", mipLevelID);
            m_pAnnotation->BeginEvent(renderPassName.c_str());
            m_PSOGenerateMipLevel.Apply(m_pImmediateContext);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVTextures), ppSRVTextures);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVTextures), ppUAVTextures, nullptr);
            m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplers), ppSamplers);
            m_pImmediateContext->Dispatch(threadGroupX, threadGroupY, threadGroupZ);

            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
            m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplerClear), ppSamplerClear);
            m_pAnnotation->EndEvent();
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
            const auto threadGroupX = static_cast<uint32_t>(std::ceil(m_DimensionX / 4.0f));
            const auto threadGroupY = static_cast<uint32_t>(std::ceil(m_DimensionY / 4.0f));
            const auto threadGroupZ = static_cast<uint32_t>(std::ceil(m_DimensionZ / 4.0f));

            ID3D11ShaderResourceView* ppSRVTextures[] = { m_pSRVVolumeIntensity[0].Get(), m_pSRVOpacityTF.Get() };
            ID3D11UnorderedAccessView* ppUAVTextures[] = { m_pUAVGradient.Get() };
            ID3D11SamplerState* ppSamplers[] = { m_pSamplerPoint.Get(), m_pSamplerLinear.Get() };

            ID3D11UnorderedAccessView* ppUAVClear[] = { nullptr };
            ID3D11ShaderResourceView* ppSRVClear[] = { nullptr, nullptr };
            ID3D11SamplerState* ppSamplerClear[] = { nullptr, nullptr };

            m_pAnnotation->BeginEvent(L"Render Pass: Compute Gradient");
            m_PSOComputeGradient.Apply(m_pImmediateContext);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVTextures), ppSRVTextures);
            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVTextures), ppUAVTextures, nullptr);
            m_pImmediateContext->Dispatch(threadGroupX, threadGroupY, threadGroupZ);

            m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
            m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
            m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplerClear), ppSamplerClear);
            m_pAnnotation->EndEvent();
        }
        m_pImmediateContext->Flush();
    }
}

void ApplicationVolumeRender::InitializeTransferFunction() {

    nlohmann::json root;
    std::ifstream file("content/TransferFunctions/ManixTransferFunction.json");
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
            v[index] = e.template get<float>();
            index++;
        }
        return v;
    };

    for (auto const& e : root["NodesColor"]) {
        const auto intensity = e["Intensity"].get<float>();
        const auto diffuse = ExtractVec3FromJson(e, "Diffuse");
        const auto specular = ExtractVec3FromJson(e, "Specular");
        const auto roughness = e["Roughness"].get<float>();

        m_DiffuseTransferFunc.AddNode(intensity, diffuse);
        m_SpecularTransferFunc.AddNode(intensity, specular);
        m_RoughnessTransferFunc.AddNode(intensity, roughness);
    }

    for (auto const& e : root["NodesOpacity"])
        m_OpacityTransferFunc.AddNode(e["Intensity"].get<F32>(), e["Opacity"].get<F32>());

    m_pSRVOpacityTF = m_OpacityTransferFunc.GenerateTexture(m_pDevice, m_SamplingCount);
    m_pSRVDiffuseTF = m_DiffuseTransferFunc.GenerateTexture(m_pDevice, m_SamplingCount);
    m_pSRVSpecularTF = m_SpecularTransferFunc.GenerateTexture(m_pDevice, m_SamplingCount);
    m_pSRVRoughnessTF = m_RoughnessTransferFunc.GenerateTexture(m_pDevice, m_SamplingCount);
}

void ApplicationVolumeRender::InitializeSamplerStates() {

    auto createSamplerState = [this](auto filter, auto addressMode) -> DX::ComPtr<ID3D11SamplerState> {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter = filter;
        desc.AddressU = addressMode;
        desc.AddressV = addressMode;
        desc.AddressW = addressMode;
        desc.MaxAnisotropy = D3D11_MAX_MAXANISOTROPY;
        desc.MaxLOD = FLT_MAX;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;

        DX::ComPtr<ID3D11SamplerState> pSamplerState;
        DX::ThrowIfFailed(m_pDevice->CreateSamplerState(&desc, pSamplerState.GetAddressOf()));
        return pSamplerState;
    };

    m_pSamplerPoint = createSamplerState(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_BORDER);
    m_pSamplerLinear = createSamplerState(D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D11_TEXTURE_ADDRESS_BORDER);
    m_pSamplerAnisotropic = createSamplerState(D3D11_FILTER_ANISOTROPIC, D3D11_TEXTURE_ADDRESS_WRAP);
}

void ApplicationVolumeRender::InitializeRenderTextures() {

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

        DX::ComPtr<ID3D11Texture2D> pTextureDiffuse;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureDiffuse.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureDiffuse.Get(), nullptr, m_pSRVDiffuse.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureDiffuse.Get(), nullptr, m_pUAVDiffuse.ReleaseAndGetAddressOf()));
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

        DX::ComPtr<ID3D11Texture2D> pTextureSpecular;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureSpecular.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureSpecular.Get(), nullptr, m_pSRVSpecular.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureSpecular.Get(), nullptr, m_pUAVSpecular.ReleaseAndGetAddressOf()));
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

        DX::ComPtr<ID3D11Texture2D> pTextureDiffuseLight;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureDiffuseLight.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureDiffuseLight.Get(), nullptr, m_pSRVRadiance.ReleaseAndGetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureDiffuseLight.Get(), nullptr, m_pUAVRadiance.ReleaseAndGetAddressOf()));
    }

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.Width = m_ApplicationDesc.Width;
        desc.Height = m_ApplicationDesc.Height;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        DX::ComPtr<ID3D11Texture2D> pTextureNormal;
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

        DX::ComPtr<ID3D11Texture2D> pTextureDepth;
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

        DX::ComPtr<ID3D11Texture2D> pTextureColorSum;
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

        DX::ComPtr<ID3D11Texture2D> pTextureToneMap;
        DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureToneMap.GetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureToneMap.Get(), nullptr, m_pSRVToneMap.GetAddressOf()));
        DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureToneMap.Get(), nullptr, m_pUAVToneMap.GetAddressOf()));
    }
}

void ApplicationVolumeRender::InitializeBuffers() {

    m_pConstantBufferFrame = DX::CreateConstantBuffer<FrameBuffer>(m_pDevice);
    m_pDispatchIndirectBufferArgs = DX::CreateIndirectBuffer<DispatchIndirectBuffer>(m_pDevice, DispatchIndirectBuffer{ 1, 1, 1 });
    m_pDrawInstancedIndirectBufferArgs = DX::CreateIndirectBuffer<DrawInstancedIndirectBuffer>(m_pDevice, DrawInstancedIndirectBuffer{ 0, 1, 0, 0 });
}

void ApplicationVolumeRender::InitializeTileBuffer() {

    const auto threadGroupsX = static_cast<uint32_t>(std::ceil(m_ApplicationDesc.Width / 8));
    const auto threadGroupsY = static_cast<uint32_t>(std::ceil(m_ApplicationDesc.Height / 8));

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

void ApplicationVolumeRender::InitializeEnvironmentMap() {

    DX::ThrowIfFailed(DirectX::CreateDDSTextureFromFile(m_pDevice.Get(), L"content/Textures/qwantani_2k.dds", nullptr, m_pSRVEnvironment.GetAddressOf()));
}

void ApplicationVolumeRender::Resize(int32_t width, int32_t height)
{
    Base::Resize(width, height);
    InitializeRenderTextures();
    InitializeTileBuffer();
    m_FrameIndex = 0;
}

void ApplicationVolumeRender::EventMouseWheel(float delta) {

    m_Zoom -= m_ZoomSensitivity * m_DeltaTime * delta;
    m_FrameIndex = 0;
}

void ApplicationVolumeRender::EventMouseMove(float x, float y) {

    m_Camera.Rotate(Hawk::Components::Camera::LocalUp, m_DeltaTime * -m_RotateSensitivity * x);
    m_Camera.Rotate(m_Camera.Right(), m_DeltaTime * -m_RotateSensitivity * y);
    m_FrameIndex = 0;
}

void ApplicationVolumeRender::Update(float deltaTime) {

    m_DeltaTime = deltaTime;

    try {
        if (m_IsReloadShader) {
            InitializeShaders();
            m_IsReloadShader = false;
        }

        if (m_IsReloadTransferFunc) {
            InitializeTransferFunction();
            m_IsReloadTransferFunc = false;
        }

    } catch (std::exception const& e) {
        std::cout << e.what() << std::endl;
    }

    Hawk::Math::Vec3 scaleVector = { 0.488f * m_DimensionX, 0.488f * m_DimensionY, 0.7f * m_DimensionZ };
    scaleVector /= (std::max)({ scaleVector.x, scaleVector.y, scaleVector.z });

    Hawk::Math::Mat4x4 V = m_Camera.ToMatrix();
    Hawk::Math::Mat4x4 P = Hawk::Math::Orthographic(m_Zoom * (m_ApplicationDesc.Width / static_cast<F32>(m_ApplicationDesc.Height)), m_Zoom, -1.0f, 1.0f);
    Hawk::Math::Mat4x4 W = Hawk::Math::RotateX(Hawk::Math::Radians(-90.0f)) * Hawk::Math::Scale(scaleVector);
    Hawk::Math::Mat4x4 N = Hawk::Math::Inverse(Hawk::Math::Transpose(W));

    Hawk::Math::Mat4x4 VP = P * V;
    Hawk::Math::Mat4x4 NV = V * N;
    Hawk::Math::Mat4x4 WVP = P * V * W;

    {
        DX::MapHelper<FrameBuffer> map(m_pImmediateContext, m_pConstantBufferFrame, D3D11_MAP_WRITE_DISCARD, 0);

        map->ProjectionMatrix = P;
        map->ViewMatrix = V;
        map->WorldMatrix = W;
        map->NormalMatrix = N;

        map->InvProjectionMatrix = Hawk::Math::Inverse(P);
        map->InvViewMatrix = Hawk::Math::Inverse(V);
        map->InvWorldMatrix = Hawk::Math::Inverse(W);
        map->InvNormalMatrix = Hawk::Math::Inverse(N);

        map->ViewProjectionMatrix = VP;
        map->NormalViewMatrix = NV;
        map->WorldViewProjectionMatrix = WVP;

        map->InvViewProjectionMatrix = Hawk::Math::Inverse(VP);
        map->InvNormalViewMatrix = Hawk::Math::Inverse(NV);
        map->InvWorldViewProjectionMatrix = Hawk::Math::Inverse(WVP);

        map->BoundingBoxMin = m_BoundingBoxMin;
        map->BoundingBoxMax = m_BoundingBoxMax;

        map->StepSize = Hawk::Math::Distance(map->BoundingBoxMin, map->BoundingBoxMax) / m_StepCount;

        map->Density = m_Density;
        map->FrameIndex = m_FrameIndex;
        map->Exposure = m_Exposure;

        map->FrameOffset = Hawk::Math::Vec2(m_RandomDistribution(m_RandomGenerator), m_RandomDistribution(m_RandomGenerator));
        map->RenderTargetDim = Hawk::Math::Vec2(static_cast<F32>(m_ApplicationDesc.Width), static_cast<F32>(m_ApplicationDesc.Height));
        map->InvRenderTargetDim = Hawk::Math::Vec2(1.0f, 1.0f) / map->RenderTargetDim;
    }
}

void ApplicationVolumeRender::TextureBlit(DX::ComPtr<ID3D11ShaderResourceView> pSrc, DX::ComPtr<ID3D11RenderTargetView> pDst) {

    ID3D11ShaderResourceView* ppSRVClear[] = { nullptr, nullptr, nullptr, nullptr,  nullptr, nullptr, nullptr, nullptr };
    D3D11_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(m_ApplicationDesc.Width), static_cast<float>(m_ApplicationDesc.Height), 0.0f, 1.0f };
    D3D11_RECT scissor = { 0, 0,static_cast<int32_t>(m_ApplicationDesc.Width), static_cast<int32_t>(m_ApplicationDesc.Height) };

    m_pImmediateContext->OMSetRenderTargets(1, pDst.GetAddressOf(), nullptr);
    m_pImmediateContext->RSSetScissorRects(1, &scissor);
    m_pImmediateContext->RSSetViewports(1, &viewport);

    // Bind PSO and Resources
    m_PSOBlit.Apply(m_pImmediateContext);
    m_pImmediateContext->PSSetShaderResources(0, 1, pSrc.GetAddressOf());
    m_pImmediateContext->PSSetSamplers(0, 1, m_pSamplerPoint.GetAddressOf());

    // Execute
    m_pImmediateContext->Draw(6, 0);

    // Unbind RTV's
    m_pImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);
    m_pImmediateContext->RSSetScissorRects(0, nullptr);
    m_pImmediateContext->RSSetViewports(0, nullptr);

    // Unbind PSO and unbind Resources
    m_PSODefault.Apply(m_pImmediateContext);
    m_pImmediateContext->PSSetSamplers(0, 0, nullptr);
    m_pImmediateContext->PSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
}

void ApplicationVolumeRender::RenderFrame(DX::ComPtr<ID3D11RenderTargetView> pRTV) {

    ID3D11UnorderedAccessView* ppUAVClear[] = { nullptr, nullptr, nullptr, nullptr };
    ID3D11ShaderResourceView* ppSRVClear[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

    const auto threadGroupsX = static_cast<uint32_t>(std::ceil(m_ApplicationDesc.Width / 8.0f));
    const auto threadGroupsY = static_cast<uint32_t>(std::ceil(m_ApplicationDesc.Height / 8.0f));

    m_pImmediateContext->VSSetConstantBuffers(0, 1, m_pConstantBufferFrame.GetAddressOf());
    m_pImmediateContext->GSSetConstantBuffers(0, 1, m_pConstantBufferFrame.GetAddressOf());
    m_pImmediateContext->PSSetConstantBuffers(0, 1, m_pConstantBufferFrame.GetAddressOf());
    m_pImmediateContext->CSSetConstantBuffers(0, 1, m_pConstantBufferFrame.GetAddressOf());

    if (m_FrameIndex < m_SampleDispersion) {
        ID3D11UnorderedAccessView* ppUAVResources[] = { m_pUAVDispersionTiles.Get() };
        constexpr uint32_t pCounters[] = { 0 };

        m_pAnnotation->BeginEvent(L"Render Pass: Reset computed tiles");
        m_PSOResetTiles.Apply(m_pImmediateContext);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVResources), ppUAVResources, pCounters);
        m_pImmediateContext->Dispatch(threadGroupsX, threadGroupsY, 1);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
        m_pAnnotation->EndEvent();
    } else {
        ID3D11ShaderResourceView* ppSRVResources[] = { m_pSRVToneMap.Get(), m_pSRVDepth.Get() };
        ID3D11UnorderedAccessView* ppUAVResources[] = { m_pUAVDispersionTiles.Get() };
        constexpr uint32_t pCounters[] = { 0 };

        m_pAnnotation->BeginEvent(L"Render Pass: Generate computed tiles");
        m_PSOComputeTiles.Apply(m_pImmediateContext);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVResources), ppSRVResources);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVResources), ppUAVResources, pCounters);
        m_pImmediateContext->Dispatch(threadGroupsX, threadGroupsY, 1);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
        m_pAnnotation->EndEvent();
    }

    constexpr float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_pAnnotation->BeginEvent(L"Render Pass: Clear buffers [Color, Normal, Depth]");
    m_pImmediateContext->ClearUnorderedAccessViewFloat(m_pUAVDiffuse.Get(), clearColor);
    m_pImmediateContext->ClearUnorderedAccessViewFloat(m_pUAVNormal.Get(), clearColor);
    m_pImmediateContext->ClearUnorderedAccessViewFloat(m_pUAVDepth.Get(), clearColor);
    m_pImmediateContext->ClearUnorderedAccessViewFloat(m_pUAVRadiance.Get(), clearColor);
    m_pAnnotation->EndEvent();

    m_pAnnotation->BeginEvent(L"Render Pass: Copy counters of tiles");
    m_pImmediateContext->CopyStructureCount(m_pDispatchIndirectBufferArgs.Get(), 0, m_pUAVDispersionTiles.Get());
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
            m_pSRVOpacityTF.Get(),
            m_pSRVDispersionTiles.Get()
        };

        ID3D11UnorderedAccessView* ppUAVResources[] = {
            m_pUAVDiffuse.Get(),
            m_pUAVSpecular.Get(),
            m_pUAVNormal.Get(),
            m_pUAVDepth.Get()
        };

        m_pAnnotation->BeginEvent(L"Render pass: Generate Rays");
        m_PSOGeneratePrimaryRays.Apply(m_pImmediateContext);
        m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplers), ppSamplers);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVResources), ppSRVResources);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVResources), ppUAVResources, nullptr);
        m_pImmediateContext->DispatchIndirect(m_pDispatchIndirectBufferArgs.Get(), 0);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
        m_pAnnotation->EndEvent();
    }

    {
        ID3D11SamplerState* ppSamplers[] = {
            m_pSamplerPoint.Get(),
            m_pSamplerLinear.Get(),
            m_pSamplerAnisotropic.Get()
        };

        ID3D11ShaderResourceView* ppSRVResources[] = {
            m_pSRVVolumeIntensity[m_MipLevel].Get(),
            m_pSRVOpacityTF.Get(),
            m_pSRVDiffuse.Get(),
            m_pSRVSpecular.Get(),
            m_pSRVNormal.Get(),
            m_pSRVDepth.Get(),
            m_pSRVEnvironment.Get(),
            m_pSRVDispersionTiles.Get()
        };

        ID3D11UnorderedAccessView* ppUAVResources[] = {
            m_pUAVRadiance.Get()
        };

        m_pAnnotation->BeginEvent(L"Render pass: Compute Radiance");
        m_PSOComputeDiffuseLight.Apply(m_pImmediateContext);
        m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplers), ppSamplers);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVResources), ppSRVResources);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVResources), ppUAVResources, nullptr);
        m_pImmediateContext->DispatchIndirect(m_pDispatchIndirectBufferArgs.Get(), 0);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
        m_pAnnotation->EndEvent();
    }

    {
        ID3D11ShaderResourceView* ppSRVResources[] = { m_pSRVRadiance.Get(),  m_pSRVDispersionTiles.Get() };
        ID3D11UnorderedAccessView* ppUAVResources[] = { m_pUAVColorSum.Get() };

        m_pAnnotation->BeginEvent(L"Render Pass: Accumulate");
        m_PSOAccumulate.Apply(m_pImmediateContext);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVResources), ppSRVResources);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVResources), ppUAVResources, nullptr);
        m_pImmediateContext->DispatchIndirect(m_pDispatchIndirectBufferArgs.Get(), 0);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
        m_pAnnotation->EndEvent();
    }

    {
        ID3D11ShaderResourceView* ppSRVResources[] = { m_pSRVColorSum.Get(), m_pSRVDispersionTiles.Get() };
        ID3D11UnorderedAccessView* ppUAVResources[] = { m_pUAVToneMap.Get() };

        m_pAnnotation->BeginEvent(L"Render Pass: Tone Map");
        m_PSOToneMap.Apply(m_pImmediateContext);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVResources), ppSRVResources);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVResources), ppUAVResources, nullptr);
        m_pImmediateContext->DispatchIndirect(m_pDispatchIndirectBufferArgs.Get(), 0);
        m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
        m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
        m_pAnnotation->EndEvent();
    }

    m_pAnnotation->BeginEvent(L"Render Pass: TextureBlit [Tone Map] -> [Back Buffer]");
    this->TextureBlit(m_pSRVToneMap, pRTV);
    m_pAnnotation->EndEvent();

    if (m_IsDrawDebugTiles) {
        ID3D11ShaderResourceView* ppSRVResources[] = { m_pSRVDispersionTiles.Get() };
        D3D11_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(m_ApplicationDesc.Width), static_cast<float>(m_ApplicationDesc.Height), 0.0f, 1.0f };

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

void ApplicationVolumeRender::RenderGUI(DX::ComPtr<ID3D11RenderTargetView> pRTV) {

    assert(ImGui::GetCurrentContext() != nullptr && "Missing dear imgui context. Refer to examples app!");

    ImGui::Begin("Settings");

    static bool isShowAppMetrics = false;
    static bool isShowAppAbout = false;

    if (isShowAppMetrics)
        ImGui::ShowMetricsWindow(&isShowAppMetrics);

    if (isShowAppAbout)
        ImGui::ShowAboutWindow(&isShowAppAbout);

    /*
    ImGui::Begin("Transfer Functions");
    if (ImPlot::BeginPlot("Opacity", 0, 0, ImVec2(-1, 175), 0, ImPlotAxisFlags_None, ImPlotAxisFlags_None)) {
        std::vector<ImVec2> opacity;
        opacity.resize(m_SamplingCount);

        for (uint32_t index = 0; index < m_SamplingCount; index++) {
            const float x = (index / static_cast<float>(m_SamplingCount - 1));
            const float y = m_OpacityTransferFunc.Evaluate(x);
            opacity[index] = ImVec2(x * (m_OpacityTransferFunc.PLF.RangeMax - m_OpacityTransferFunc.PLF.RangeMin) + m_OpacityTransferFunc.PLF.RangeMin, y);
        }
        ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
        ImPlot::PlotShaded("Opacity", &opacity[0].x, &opacity[0].y, std::size(opacity), 0, 0, sizeof(ImVec2));
        ImPlot::PopStyleVar();

        ImPlot::PlotLine("Opacity", &opacity[0].x, &opacity[0].y, std::size(opacity), 0, sizeof(ImVec2));
        ImPlot::EndPlot();
    }
    ImGui::End();
    */

    if (ImGui::CollapsingHeader("Camera")) {
        ImGui::SliderFloat("Rotate sensitivity", &m_RotateSensitivity, 0.1f, 10.0f);
        ImGui::SliderFloat("Zoom sensitivity", &m_ZoomSensitivity, 0.1f, 10.0f);
    }

    if (ImGui::CollapsingHeader("Volume")) {
        ImGui::SliderFloat("Density", &m_Density, 0.1f, 100.0f);
        ImGui::SliderInt("Step count", reinterpret_cast<int32_t*>(&m_StepCount), 1, 512);
        m_FrameIndex = ImGui::SliderInt("Mip Level", reinterpret_cast<int32_t*>(&m_MipLevel), 0, m_DimensionMipLevels - 1) ? 0 : m_FrameIndex;
    }

    if (ImGui::CollapsingHeader("Post-Processing"))
        ImGui::SliderFloat("Exposure", &m_Exposure, 4.0f, 100.0f);

    if (ImGui::CollapsingHeader("Debug"))
        ImGui::Checkbox("Show computed tiles", &m_IsDrawDebugTiles);

    ImGui::Checkbox("Show metrics", &isShowAppMetrics);
    ImGui::Checkbox("Show about", &isShowAppAbout);

    ImGui::End();
}

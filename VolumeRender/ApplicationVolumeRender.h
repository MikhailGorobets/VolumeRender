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

#pragma once

#include "Application.h"
#include "TransferFunction.h"

#include <Hawk/Components/Camera.hpp>
#include <Hawk/Math/Functions.hpp>
#include <Hawk/Math/Transform.hpp>
#include <Hawk/Math/Converters.hpp>

class ApplicationVolumeRender final : public Application {
public:
    ApplicationVolumeRender(ApplicationDesc const& desc);
private:
    auto InitializeVolumeTexture() -> void;

    auto InitializeTransferFunction() -> void;

    auto InitializeSamplerStates() -> void;

    auto InitializeShaders() -> void;

    auto InitializeRenderTextures() -> void;

    auto InitializeConstantBuffers() -> void;

    auto InitializeEnviromentMap() -> void;

    auto EventMouseWheel(float delta) -> void override;

    auto EventMouseMove(float x, float y) -> void override;

    auto Update(float deltaTime) -> void override;

    auto Blit(DX::ComPtr<ID3D11ShaderResourceView> pSrc, DX::ComPtr<ID3D11RenderTargetView> pDst) -> void;

    auto RenderFrame(DX::ComPtr<ID3D11RenderTargetView> pRTV) -> void override;

    auto RenderGUI(DX::ComPtr<ID3D11RenderTargetView> pRTV) -> void override;

private:
    DX::ComPtr<ID3D11ShaderResourceView>  m_pSRVVolumeIntensity;

    DX::ComPtr<ID3D11ShaderResourceView> m_pSRVDiffuseTF;
    DX::ComPtr<ID3D11ShaderResourceView> m_pSRVSpecularTF;
    DX::ComPtr<ID3D11ShaderResourceView> m_pSRVRoughnessTF;
    DX::ComPtr<ID3D11ShaderResourceView> m_pSRVOpasityTF;
    DX::ComPtr<ID3D11ShaderResourceView> m_pSRVEnviroment;

    DX::ComPtr<ID3D11ShaderResourceView>  m_pSRVColor;
    DX::ComPtr<ID3D11ShaderResourceView>  m_pSRVColorSum;

    DX::ComPtr<ID3D11UnorderedAccessView> m_pUAVColor;
    DX::ComPtr<ID3D11UnorderedAccessView> m_pUAVColorSum;

    DX::ComPtr<ID3D11ShaderResourceView>  m_pSRVToneMap;
    DX::ComPtr<ID3D11UnorderedAccessView> m_pUAVToneMap;

    DX::GraphicsPSO m_PSODefault = {};
    DX::GraphicsPSO m_PSOBlit = {};
    DX::ComputePSO  m_PSOPathTracing = {};
    DX::ComputePSO  m_PSOPathTracingSum = {};
    DX::ComputePSO  m_PSOToneMap = {};

    DX::ComPtr<ID3D11SamplerState>  m_pSamplerPoint;
    DX::ComPtr<ID3D11SamplerState>  m_pSamplerLinear;
    DX::ComPtr<ID3D11SamplerState>  m_pSamplerAnisotropic;

    DX::ComPtr<ID3D11Buffer> m_pCBFrame;

    ColorTransferFunction1D  m_DiffuseTransferFunc;
    ColorTransferFunction1D  m_SpecularTransferFunc;
    ColorTransferFunction1D  m_EmissionTransferFunc;
    ScalarTransferFunction1D m_RoughnessTransferFunc;
    ScalarTransferFunction1D m_OpacityTransferFunc;

    Hawk::Components::Camera m_Camera;

    Hawk::Math::Vec3 m_BoundingBoxMin = Hawk::Math::Vec3(-0.5f, -0.5f, -0.5f);
    Hawk::Math::Vec3 m_BoundingBoxMax = Hawk::Math::Vec3(+0.5f, +0.5f, +0.5f);

    float    m_DeltaTime = 0.0f;
    float    m_RotateSensivity = 0.25f;
    float    m_ZoomSensivity = 1.5f;
    float    m_Density = 100.0f;
    float    m_Exposure = 20.0f;
    float    m_DenoiserStrange = 1.0f;
    float    m_Zoom = 1.0f;
    uint32_t m_TraceDepth = 2;
    uint32_t m_StepCount = 180;
    uint32_t m_FrameIndex = 0;
    uint32_t m_SamplingCount = 256;

    bool     m_IsReloadShader = false;
    bool     m_IsReloadTranferFunc = false;

    uint16_t m_DimensionX = 0;
    uint16_t m_DimensionY = 0;
    uint16_t m_DimensionZ = 0;

    uint32_t m_ThreadSizeDimensionX = 16;
    uint32_t m_ThreadSizeDimensionY = 16;

    std::random_device m_RandomDevice;
    std::mt19937       m_RandomGenerator;
    std::uniform_real_distribution<float> m_RandomDistribution;
};
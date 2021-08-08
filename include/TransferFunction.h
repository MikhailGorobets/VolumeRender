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

#include "Common.h"

#include <Hawk/Math/Functions.hpp>
#include <Hawk/Math/Transform.hpp>
#include <Hawk/Math/Converters.hpp>

template<uint32_t N>
struct PiewiseFunction {
    F32                RangeMin = -1024.0f;
    F32                RangeMax = +3071.0f;
    uint32_t           Count = 0;
    std::array<F32, N> Position;
    std::array<F32, N> Value;
};

template<uint32_t N = 64>
class PiecewiseLinearFunction :public PiewiseFunction<N> {
public:
    auto AddNode(F32 position, F32 value) -> void {
        this->Position[this->Count] = position;
        this->Value[this->Count] = value;
        this->Count++;
    }

    auto Evaluate(F32 positionNormalized) const -> F32 {
        auto position = positionNormalized * (this->RangeMax - this->RangeMin) + this->RangeMin;

        if (this->Count <= 0)
            return 0.0f;

        if (position < this->RangeMin)
            return this->Value[0];

        if (position > this->RangeMax)
            return this->Value[this->Count - 1];

        for (size_t i = 1; i < this->Count; i++) {
            auto const p1 = this->Position[i - 1];
            auto const p2 = this->Position[i];
            auto const t = (position - p1) / (p2 - p1);

            if (position >= p1 && position < p2)
                return this->Value[i - 1] + t * (this->Value[i] - this->Value[i - 1]);
        }
        return 0.0f;
    }

    auto Clear() -> void {
        this->Count = 0;
    }
};

class ScalarTransferFunction1D {
public:
    auto AddNode(F32 position, F32 value) -> void { this->PLF.AddNode(position, value); }

    auto Evaluate(F32 intensity) -> F32 { return this->PLF.Evaluate(intensity); }

    auto GenerateTexture(Microsoft::WRL::ComPtr<ID3D11Device> pDevice, uint32_t sampling = 64) -> Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> {
        std::vector<uint8_t> data(sampling);
        for (auto index = 0u; index < sampling; index++)
            data[index] = static_cast<uint8_t>(std::round(255.0f * this->Evaluate(index / static_cast<F32>(sampling - 1))));

        D3D11_TEXTURE1D_DESC desc = {};
        desc.Width = sampling;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.Format = DXGI_FORMAT_R8_UNORM;
        desc.Usage = D3D11_USAGE_IMMUTABLE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = std::data(data);

        Microsoft::WRL::ComPtr<ID3D11Texture1D> pTexture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pSRV;
        DX::ThrowIfFailed(pDevice->CreateTexture1D(&desc, &initData, pTexture.GetAddressOf()));
        DX::ThrowIfFailed(pDevice->CreateShaderResourceView(pTexture.Get(), nullptr, pSRV.GetAddressOf()));

        return pSRV;
    }

    auto Clear() -> void { this->PLF.Clear(); }

    PiecewiseLinearFunction<> PLF;
};

class ColorTransferFunction1D {
public:
    auto AddNode(F32 position, Hawk::Math::Vec3 value) -> void {
        this->PLF[0].AddNode(position, value.x);
        this->PLF[1].AddNode(position, value.y);
        this->PLF[2].AddNode(position, value.z);
    }

    auto Evaluate(F32 intensity) ->  Hawk::Math::Vec3 {
        return Hawk::Math::Vec3(this->PLF[0].Evaluate(intensity), this->PLF[1].Evaluate(intensity), this->PLF[2].Evaluate(intensity));
    }

    auto GenerateTexture(Microsoft::WRL::ComPtr<ID3D11Device> pDevice, uint32_t sampling = 64) -> Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> {
        std::vector<Hawk::Math::Vector<uint8_t, 4>> data(sampling);
        for (size_t index = 0; index < sampling; index++) {
            Hawk::Math::Vec3 v = this->Evaluate(index / static_cast<F32>(sampling - 1));
            uint8_t x = static_cast<uint8_t>(std::round(255.0f * v.x));
            uint8_t y = static_cast<uint8_t>(std::round(255.0f * v.y));
            uint8_t z = static_cast<uint8_t>(std::round(255.0f * v.z));
            data[index] = Hawk::Math::Vector<uint8_t, 4>( x, y, z, static_cast<uint8_t>(0));
        }
         
        D3D11_TEXTURE1D_DESC desc = {};
        desc.Width = sampling;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Usage = D3D11_USAGE_IMMUTABLE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = std::data(data);

        Microsoft::WRL::ComPtr<ID3D11Texture1D> pTexture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pSRV;
        DX::ThrowIfFailed(pDevice->CreateTexture1D(&desc, &initData, pTexture.GetAddressOf()));
        DX::ThrowIfFailed(pDevice->CreateShaderResourceView(pTexture.Get(), nullptr, pSRV.GetAddressOf()));

        return pSRV;
    }

    auto Clear() -> void {
        this->PLF[0].Clear();
        this->PLF[1].Clear();
        this->PLF[2].Clear();
    }

    std::array<PiecewiseLinearFunction<>, 3> PLF;
};
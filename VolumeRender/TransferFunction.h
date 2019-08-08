#pragma once

#include "Common.h"

template<uint32_t N>
struct PiewiseFunction {
	Hawk::Math::Vec2   Range = { (std::numeric_limits<F32>::min)(), (std::numeric_limits<F32>::max)() };
	uint32_t           Count = 0;
	std::array<F32, N> Position;
	std::array<F32, N> Value;
};

template<uint32_t N = 64>
class PiecewiseLinearFunction : PiewiseFunction<N> {
public:
	auto AddNode(F32 position, F32 value) -> void {

		this->Position[this->Count] = position;
		this->Value[this->Count] = value;

		if (position < this->Range.x)
			this->Range.x = position;

		if (position > this->Range.y)
			this->Range.y = position;

		this->Count++;
	}

	auto Evaluate(F32 position) const -> F32 {

		if (this->Count <= 0)
			return 0.0f;

		if (position < this->Range.x)
			return this->Value[0];

		if (position > this->Range.y)
			return this->Value[this->Count - 1];

		for (auto i = 1u; i < this->Count; i++) {
			auto const p1 = this->Position[i - 1];
			auto const p2 = this->Position[i];
			auto const t = (position - p1) / (p2 - p1);

			if (position >= p1 && position < p2)
				return this->Value[i - 1] + t * (this->Value[i] - this->Value[i - 1]);
		}
		return 0.0f;
	}

	auto Clear() -> void {
		this->Range = { (std::numeric_limits<F32>::min)(), (std::numeric_limits<F32>::max)() };
		this->Count = 0;
	}
};

class ScalarTransferFunction1D {
public:

	auto AddNode(F32 position, F32 value) -> void {
		this->PLF.AddNode(position, value);
	}

	auto Evaluate(F32 intensity) -> F32 {
		return this->PLF.Evaluate(intensity);
	}

	auto GenerateTexture(Microsoft::WRL::ComPtr<ID3D11Device> pDevice, uint32_t sampling = 64) -> Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> {
	

		std::vector<F32> data(sampling);
		for (auto index = 0u; index < sampling; index++)
			data[index] = this->Evaluate(index / static_cast<F32>(sampling));



		D3D11_TEXTURE1D_DESC desc = {};
		desc.Width = sampling;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Format = DXGI_FORMAT_R32_FLOAT;
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
		this->PLF.Clear();
	}

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
		return  Hawk::Math::Vec3(this->PLF[0].Evaluate(intensity), this->PLF[1].Evaluate(intensity), this->PLF[2].Evaluate(intensity));
	}


	auto GenerateTexture(Microsoft::WRL::ComPtr<ID3D11Device> pDevice, uint32_t sampling = 64) -> Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> {

	

		std::vector<Hawk::Math::Vec4> data(sampling);
		for (auto index = 0u; index < sampling; index++)
			data[index] = Hawk::Math::Vec4(this->Evaluate(index / static_cast<F32>(sampling)), 0.0f);
		

		D3D11_TEXTURE1D_DESC desc = {};
		desc.Width = sampling;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
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
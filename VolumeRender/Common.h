#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <iostream>
#include <fstream>
#include <array>
#include <numeric>
#include <tuple>
#include <cassert>
#include <random>

#include <dxgi.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <wrl.h>

namespace DX {
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

	class com_exception : public std::exception {
	public:
		com_exception(HRESULT hr) : result(hr) {}

		virtual const char* what() const override {
			static char s_str[64] = {};
			sprintf_s(s_str, "Failure with HRESULT of %08X",
				static_cast<unsigned int>(result));
			return s_str;
		}

	private:
		HRESULT result;
	};

	inline void ThrowIfFailed(HRESULT hr) {
		if (FAILED(hr))	 throw com_exception(hr);
	}

	template<typename T>
	auto CreateConstantBuffer(ComPtr<ID3D11Device> pDevice) -> Microsoft::WRL::ComPtr<ID3D11Buffer> {
		ComPtr<ID3D11Buffer> pBuffer;
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(T);
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		ThrowIfFailed(pDevice->CreateBuffer(&desc, nullptr, pBuffer.GetAddressOf()));
		return pBuffer;
	}

	template<typename T>
	auto CreateStructuredBuffer(ComPtr<ID3D11Device> pDevice, uint32_t numElements, bool isCPUWritable, bool isGPUWritable,	const T* pInitialData = nullptr ) -> Microsoft::WRL::ComPtr<ID3D11Buffer> {
		ComPtr<ID3D11Buffer> pBuffer;

		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(T) * numElements;
		if ((!isCPUWritable) && (!isGPUWritable)) {
			desc.CPUAccessFlags = 0;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.Usage = D3D11_USAGE_IMMUTABLE;
		} else if (isCPUWritable && (!isGPUWritable)) {
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.Usage = D3D11_USAGE_DYNAMIC;
		}
		else if ((!isCPUWritable) && isGPUWritable) {
			desc.CPUAccessFlags = 0;
			desc.BindFlags = (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
			desc.Usage = D3D11_USAGE_DEFAULT;
		} else {
			assert((!(isCPUWritable && isGPUWritable)));
		}

		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(T);

		D3D11_SUBRESOURCE_DATA data = {};
		data.pSysMem = pInitialData;
		ThrowIfFailed(pDevice->CreateBuffer((&desc), (pInitialData) ? (&data) : nullptr, pBuffer.GetAddressOf()));		
		return pBuffer;
	}
	
	template<typename DataType>
	class MapHelper {
	public:
		MapHelper() {}

		MapHelper(ComPtr<ID3D11DeviceContext> pContext, ComPtr<ID3D11Buffer> pBuffer, D3D11_MAP mapType, uint32_t mapFlags)
			: MapHelper() {		
			Map(pContext, pBuffer, mapType, mapFlags);
		}
        
        MapHelper(const MapHelper&) = delete;

        MapHelper& operator=(const MapHelper&) = delete;

		MapHelper(MapHelper&& rhs) 
            : m_pBuffer(std::move(rhs.m_pBuffer))
            , m_pMappedData(std::move(rhs.m_pMappedData))
			, m_pContext(std::move(rhs.m_pContext))
			, m_MapType(std::move(rhs.m_MapType))
			, m_MapFlags(std::move(rhs.m_MapFlags)) {
		    rhs.m_pBuffer = nullptr;
		    rhs.m_pContext = nullptr;
		    rhs.m_pMappedData = nullptr;
		    rhs.m_MapType = static_cast<MAP_TYPE>(-1);
		    rhs.m_MapFlags = static_cast<Uint32>(-1);
		}
    
        MapHelper& operator = (MapHelper&& rhs) {
            m_pBuffer = std::move(rhs.m_pBuffer);
            m_pMappedData = std::move(rhs.m_pMappedData);
            m_pContext = std::move(rhs.m_pContext);
            m_MapType = std::move(rhs.m_MapType);
            m_MapFlags = std::move(rhs.m_MapFlags);
            rhs.m_pBuffer = nullptr;
            rhs.m_pContext = nullptr;
            rhs.m_pMappedData = nullptr;
            rhs.m_MapType = static_cast<D3D11_MAP>(-1);
            rhs.m_MapFlags = static_cast<uint32_t>(-1);
            return *this;
        }
        
        ~MapHelper() { Unmap(); }

		void Map(ComPtr<ID3D11DeviceContext> pContext, ComPtr<ID3D11Buffer> pBuffer, D3D11_MAP mapType, uint32_t mapFlags) {
		//	assert(!m_pBuffer && !m_pMappedData && !m_pContext, "Object already mapped");
			Unmap();
#ifdef _DEBUG
			D3D11_BUFFER_DESC desc;
			pBuffer->GetDesc(&desc);
		//	assert(sizeof(DataType) <= BuffDesc.uiSizeInBytes, "Data type size exceeds buffer size");
#endif
			D3D11_MAPPED_SUBRESOURCE resource = {};
			ThrowIfFailed(pContext->Map(pBuffer.Get(), 0, mapType, mapFlags, &resource));
			m_pMappedData = static_cast<DataType*>(resource.pData);
			if (m_pMappedData != nullptr) {
				m_pContext = pContext;
				m_pBuffer = pBuffer;
				m_MapType = mapType;
				m_MapFlags = mapFlags;
			}
		}

		auto Unmap() -> void {
			if (m_pBuffer) {
				m_pContext->Unmap(m_pBuffer.Get(), 0);
				m_pBuffer = nullptr;
				m_MapType = static_cast<D3D11_MAP>(-1);
				m_MapFlags = static_cast<uint32_t>(-1);
			}
			m_pContext = nullptr;
			m_pMappedData = nullptr;
		}

		operator DataType* () { return m_pMappedData; }

		operator const DataType* () const { return m_pMappedData; }

		auto operator->() -> DataType* { return m_pMappedData; }
	
		auto operator->() const -> const DataType* { return m_pMappedData; }

	private:
		ComPtr<ID3D11Buffer>        m_pBuffer  = nullptr;
		ComPtr<ID3D11DeviceContext> m_pContext = nullptr;
		DataType* m_pMappedData = nullptr;
		D3D11_MAP m_MapType  = static_cast<D3D11_MAP>(-1);
		uint32_t  m_MapFlags = static_cast<uint32_t>(-1);
	};
    
    class GraphicsPSO {
    public:
        auto Apply(ComPtr<ID3D11DeviceContext> pDeviceContext) const -> void {
            pDeviceContext->IASetPrimitiveTopology(PrimitiveTopology);
            pDeviceContext->IASetInputLayout(pInputLayout.Get());
            pDeviceContext->VSSetShader(pVS.Get(), nullptr, 0);
            pDeviceContext->PSSetShader(pPS.Get(), nullptr, 0);
            pDeviceContext->RSSetState(pRasterState.Get());
            pDeviceContext->OMSetDepthStencilState(pDepthStencilState.Get(), 0);
            pDeviceContext->OMSetBlendState(pBlendState.Get(), nullptr, BlendMask);
        }

    public:
        ComPtr<ID3D11InputLayout>       pInputLayout = nullptr;
        ComPtr<ID3D11VertexShader>      pVS = nullptr;
        ComPtr<ID3D11PixelShader>       pPS = nullptr;
        ComPtr<ID3D11RasterizerState>   pRasterState = nullptr;
        ComPtr<ID3D11DepthStencilState> pDepthStencilState = nullptr;
        ComPtr<ID3D11BlendState>        pBlendState = nullptr;
        uint32_t                        BlendMask = 0xFFFFFFFF;
        D3D11_PRIMITIVE_TOPOLOGY        PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    };

    class ComputePSO {
    public:
        auto Apply(ComPtr<ID3D11DeviceContext> pDeviceContext) const -> void {
            pDeviceContext->CSSetShader(pCS.Get(), nullptr, 0);
        }

    public:
        ComPtr<ID3D11ComputeShader> pCS = nullptr;
    };
}
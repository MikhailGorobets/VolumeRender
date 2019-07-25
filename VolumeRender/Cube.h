#pragma once

class Cube {


public:
	Cube(Microsoft::WRL::ComPtr<ID3D11Device> pDevice, F32 size) {


		{

			Hawk::Math::Vec3 vertices[] = {
				//Top
			   Hawk::Math::Vec3(-size, +size, -size),
			   Hawk::Math::Vec3(+size, +size, -size),
			   Hawk::Math::Vec3(+size, +size, +size),
			   Hawk::Math::Vec3(-size, +size, +size),
			   // Bottom                                                             
			   Hawk::Math::Vec3(-size,-size, +size),
			   Hawk::Math::Vec3(+size,-size, +size),
			   Hawk::Math::Vec3(+size,-size, -size),
			   Hawk::Math::Vec3(-size,-size, -size),
			   // Left                                           
			   Hawk::Math::Vec3(-size, +size, -size),
			   Hawk::Math::Vec3(-size, +size, +size),
			   Hawk::Math::Vec3(-size, -size, +size),
			   Hawk::Math::Vec3(-size, -size, -size),
			   // Right                     
			   Hawk::Math::Vec3(+size, +size, +size),
			   Hawk::Math::Vec3(+size, +size, -size),
			   Hawk::Math::Vec3(+size, -size, -size),
			   Hawk::Math::Vec3(+size, -size, +size),
			   // Back                        
			   Hawk::Math::Vec3(+size, +size, -size),
			   Hawk::Math::Vec3(-size, +size, -size),
			   Hawk::Math::Vec3(-size, -size, -size),
			   Hawk::Math::Vec3(+size, -size, -size),
			   // Front                       
			   Hawk::Math::Vec3(-size, +size, +size),
			   Hawk::Math::Vec3(+size, +size, +size),
			   Hawk::Math::Vec3(+size, -size, +size),
			   Hawk::Math::Vec3(-size, -size, +size)

			};

			m_VertexCount = _countof(vertices);

			D3D11_BUFFER_DESC desc = {};
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.ByteWidth = sizeof(vertices);
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

			D3D11_SUBRESOURCE_DATA data =  {};
			data.pSysMem = vertices;
		
			DX::ThrowIfFailed(pDevice->CreateBuffer(&desc, &data, m_pVertexBuffer.GetAddressOf()));
		}

		{

			uint32_t indices[] = {
			  0,1,2, 0,2,3,
			  4,5,6, 4,6,7,
			  8,9,10, 8,10,11,
			  12,13,14, 12,14,15,
			  16,17,18, 16,18,19,
			  20,21,22, 20,22,23
			};

			m_IndexCount = _countof(indices);

			D3D11_BUFFER_DESC desc = {};
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.ByteWidth = sizeof(indices);
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

			D3D11_SUBRESOURCE_DATA data = {};
			data.pSysMem = indices;

			DX::ThrowIfFailed(pDevice->CreateBuffer(&desc, &data, m_pIndexBuffer.GetAddressOf()));
		}
	
	}

	auto Render(Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext) -> void {

		uint32_t stride = sizeof(Hawk::Math::Vec3);
		uint32_t offset = 0;

		pContext->IASetVertexBuffers(0, 1, m_pVertexBuffer.GetAddressOf(), &stride, &offset);
		pContext->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT::DXGI_FORMAT_R32_UINT, 0);
		pContext->DrawIndexed(m_IndexCount, 0, 0);
	}

private:
	uint32_t                                  m_IndexCount;
	uint32_t                                  m_VertexCount;
	Microsoft::WRL::ComPtr<ID3D11Buffer>      m_pVertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer>      m_pIndexBuffer;

};

#pragma once

#include "Application.h"
#include "TransferFunction.h"
#include "Cube.h"


struct FrameBuffer {
	Hawk::Math::Mat4x4 InvViewProjection;
	Hawk::Math::Mat4x4 BoundingBoxRotation;


	F32                BoundingBoxSize;
	uint32_t           FrameIndex;
	uint32_t           TraceDepth;
	uint32_t           StepCount;

	F32                Density;
	Hawk::Math::Vec3   BoundingBoxMin;

	F32                Exposure;
	Hawk::Math::Vec3   BoundingBoxMax;

	
	F32                IBLScale;
	Hawk::Math::Vec3   _Padding;

	Hawk::Math::Vec3   GradientDelta;
	F32                DenoiserStrange;

	F32                IsEnableEnviroment;
	F32                Gamma;
	Hawk::Math::Vec2   FrameOffset;
	
};




class ApplicationVolumeRender final : public Application {
public:
	ApplicationVolumeRender(ApplicationDesc const& desc)
		: Application(desc)
		, m_RandomGenerator(m_RandomDevice())  
		, m_RandomDistribution(0.0f, 1.0f){

		this->InitializeShaders();
		this->InitializeVolumeTexture();
		this->InitializeTransferFunction();
		this->InitializeSamplerStates();
		this->InitializeRenderTextures();
		this->InitializeConstantBuffers();
		this->InitializeEnviromentMap();
		

		m_Camera.SetTranslation(0.0f, 0.0f, 1.0f);
		m_BoundingRotation = Hawk::Math::Vec3(-100.0f, 0.0f, 0.0f);
		m_pCube = std::make_shared<Cube>(m_pDevice, 1.0f);
		

	}

private:

	auto InitializeVolumeTexture() -> void {

		
		std::ifstream file("Data/Textures/manix.dat", std::ifstream::binary);
		if (!file.is_open())
			throw std::runtime_error("Failed to open file: " + std::string("Data/Textures/manix.dat"));

		
		file.read(reinterpret_cast<char*>(&m_DimensionX), sizeof(uint16_t));
		file.read(reinterpret_cast<char*>(&m_DimensionY), sizeof(uint16_t));
		file.read(reinterpret_cast<char*>(&m_DimensionZ), sizeof(uint16_t));

		std::vector<uint16_t> data(m_DimensionX * m_DimensionY * m_DimensionZ);
		std::vector<F32> intensity(m_DimensionX * m_DimensionY * m_DimensionZ);

		uint16_t tmin = (std::numeric_limits<uint16_t>::max)();
		uint16_t tmax = (std::numeric_limits<uint16_t>::min)();

		for (auto index = 0u; index < std::size(data); index++) {
			file.read(reinterpret_cast<char*>(&data[index]), sizeof(uint16_t));
			tmin = (std::min)(tmin, data[index]);
			tmax = (std::max)(tmax, data[index]);
		}

		for (auto index = 0u; index < std::size(intensity); index++)
			intensity[index] = (data[index] - tmin) / static_cast<F32>(tmax - tmin);
		
		

		
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> pUAVVolumeGradient;

		{

			D3D11_TEXTURE3D_DESC desc = {};
			desc.Width = m_DimensionX;
			desc.Height = m_DimensionY;
			desc.Depth = m_DimensionZ;
			desc.Format = DXGI_FORMAT_R32_FLOAT;
			desc.MipLevels = 1;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.Usage = D3D11_USAGE_IMMUTABLE;


			D3D11_SUBRESOURCE_DATA resourceData = {};
			resourceData.pSysMem = std::data(intensity);
			resourceData.SysMemPitch = sizeof(F32) * desc.Width;
			resourceData.SysMemSlicePitch = sizeof(F32) * desc.Height * desc.Width;


			Microsoft::WRL::ComPtr<ID3D11Texture3D> pTextureIntensity;
			DX::ThrowIfFailed(m_pDevice->CreateTexture3D(&desc, &resourceData, pTextureIntensity.GetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureIntensity.Get(), nullptr, m_pSRVVolumeIntensity.GetAddressOf()));

		}

		{
		
			
			D3D11_TEXTURE3D_DESC desc = {};
			desc.Width  = m_DimensionX;
			desc.Height = m_DimensionY;
			desc.Depth  = m_DimensionZ;
			desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			desc.MipLevels = 1;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			desc.Usage = D3D11_USAGE_DEFAULT;


			
			Microsoft::WRL::ComPtr<ID3D11Texture3D> pTextureGradient;
			DX::ThrowIfFailed(m_pDevice->CreateTexture3D(&desc, nullptr, pTextureGradient.GetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureGradient.Get(), nullptr, m_pSRVVolumeGradient.GetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureGradient.Get(), nullptr, pUAVVolumeGradient.GetAddressOf()));

		}

		ID3D11UnorderedAccessView* ppUAVClear[] = {
			nullptr
		};

		ID3D11ShaderResourceView* ppSRVClear[] = {
			nullptr
		};

		auto threadGroupsX = static_cast<uint32_t>(std::ceil(m_DimensionX / 8.0f));
		auto threadGroupsY = static_cast<uint32_t>(std::ceil(m_DimensionY / 8.0f));
		auto threadGroupsZ = static_cast<uint32_t>(std::ceil(m_DimensionZ / 8.0f));

		m_pImmediateContext->CSSetShader(m_pCSComputeGradient.Get(), nullptr, 0);
		m_pImmediateContext->CSSetShaderResources(0, 1, m_pSRVVolumeIntensity.GetAddressOf());
		m_pImmediateContext->CSSetUnorderedAccessViews(0, 1, pUAVVolumeGradient.GetAddressOf(), nullptr);
		m_pImmediateContext->Dispatch(threadGroupsX, threadGroupsY, threadGroupsZ);
		m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
		m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
	
	}

	auto InitializeTransferFunction() -> void {
	
		boost::property_tree::ptree root;
		boost::property_tree::read_json("Data/TransferFunction/ManixTransferFunction.json", root);
		
		m_OpacityTransferFunc.Clear();
		m_DiffuseTransferFunc.Clear();
		m_SpecularTransferFunc.Clear();
		m_RoughnessTransferFunc.Clear();

		auto extractFunction = [](auto const& tree, auto const& key) -> Hawk::Math::Vec3 {
			Hawk::Math::Vec3 v;
			auto index = 0;
			for (auto& e : tree.get_child(key)) {
				v[index] = e.second.get_value<F32>() / 255.0f;
				index++;
			}
			return v;
			
		};

		for (auto const& e : root.get_child("Nodes")) {

		

			auto instensity = e.second.get<F32>("Intensity");
			auto opacity    = e.second.get<F32>("Opacity");
			auto diffuse    = extractFunction(e.second, "Diffuse");
			auto specular   = extractFunction(e.second, "Specular");
			auto roughness  = e.second.get<F32>("Roughness");

			m_OpacityTransferFunc.AddNode(instensity, opacity);
			m_DiffuseTransferFunc.AddNode(instensity, diffuse);
			m_SpecularTransferFunc.AddNode(instensity, specular);
			m_RoughnessTransferFunc.AddNode(instensity, roughness);
			

		}

		

		m_pSRVOpasityTF   = m_OpacityTransferFunc.GenerateTexture(m_pDevice);
		m_pSRVDiffuseTF   = m_DiffuseTransferFunc.GenerateTexture(m_pDevice);
		m_pSRVSpecularTF  = m_SpecularTransferFunc.GenerateTexture(m_pDevice);
		m_pSRVRoughnessTF = m_RoughnessTransferFunc.GenerateTexture(m_pDevice);
		


	}

	auto InitializeSamplerStates() -> void {
		

		auto createSamplerState = [this](auto filter, auto addressMode) -> Microsoft::WRL::ComPtr<ID3D11SamplerState>
		{
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

	auto InitializeShaders() -> void {


		auto compileShader = [](auto fileName, auto entrypoint, auto target) -> Microsoft::WRL::ComPtr<ID3DBlob> {

			Microsoft::WRL::ComPtr<ID3DBlob> pCodeBlob;
			Microsoft::WRL::ComPtr<ID3DBlob> pErrorBlob;

			
			if (FAILED(D3DCompileFromFile(fileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entrypoint, target, 0, 0, pCodeBlob.GetAddressOf(), pErrorBlob.GetAddressOf())))
				throw std::runtime_error(static_cast<const char*>(pErrorBlob->GetBufferPointer()));
			return pCodeBlob;
		};

		auto pBlobCSGradient = compileShader(L"Data/Shaders/ComputeGradient.hlsl", "ComputeGradient", "cs_5_0");
		auto pBlobCSFirstPass = compileShader(L"Data/Shaders/PathTracing.hlsl", "FirstPass", "cs_5_0");
		auto pBlobCSSecondPass = compileShader(L"Data/Shaders/PathTracing.hlsl", "SecondPass", "cs_5_0");
		auto pBlobCSToneMap = compileShader(L"Data/Shaders/ToneMap.hlsl", "ToneMap", "cs_5_0");
		auto pBlobCSDenoiser = compileShader(L"Data/Shaders/Denoiser.hlsl", "Denoiser", "cs_5_0");
		auto pBlobVSBlit = compileShader(L"Data/Shaders/Blitting.hlsl", "FullQuad", "vs_5_0");
		auto pBlobPSBlit = compileShader(L"Data/Shaders/Blitting.hlsl", "Blitting", "ps_5_0");		
		auto pBlobVSCube = compileShader(L"Data/Shaders/BoundingBox.hlsl", "VS_Main", "vs_5_0");
		auto pBlobPSCube = compileShader(L"Data/Shaders/BoundingBox.hlsl", "PS_Main", "ps_5_0");
		

		DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSGradient->GetBufferPointer(), pBlobCSGradient->GetBufferSize(), nullptr, m_pCSComputeGradient.ReleaseAndGetAddressOf()));
		DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSFirstPass->GetBufferPointer(), pBlobCSFirstPass->GetBufferSize(), nullptr, m_pCSPathTracingFirstPass.ReleaseAndGetAddressOf()));
		DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSSecondPass->GetBufferPointer(), pBlobCSSecondPass->GetBufferSize(), nullptr, m_pCSPathTracingSecondPass.ReleaseAndGetAddressOf()));
		DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSToneMap->GetBufferPointer(), pBlobCSToneMap->GetBufferSize(), nullptr, m_pCSToneMap.ReleaseAndGetAddressOf()));
		DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSDenoiser->GetBufferPointer(), pBlobCSDenoiser->GetBufferSize(), nullptr, m_pCSDenoiser.ReleaseAndGetAddressOf()));
		DX::ThrowIfFailed(m_pDevice->CreateVertexShader(pBlobVSBlit->GetBufferPointer(), pBlobVSBlit->GetBufferSize(), nullptr, m_pVSBlit.ReleaseAndGetAddressOf()));
		DX::ThrowIfFailed(m_pDevice->CreatePixelShader(pBlobPSBlit->GetBufferPointer(), pBlobPSBlit->GetBufferSize(), nullptr, m_pPSBlit.ReleaseAndGetAddressOf()));
		DX::ThrowIfFailed(m_pDevice->CreateVertexShader(pBlobVSCube->GetBufferPointer(), pBlobVSCube->GetBufferSize(), nullptr, m_pVSRenderCube.ReleaseAndGetAddressOf()));
		DX::ThrowIfFailed(m_pDevice->CreatePixelShader(pBlobPSCube->GetBufferPointer(), pBlobPSCube->GetBufferSize(), nullptr,  m_pPSRenderCube.ReleaseAndGetAddressOf()));

		{
			D3D11_INPUT_ELEMENT_DESC desc[] = {
				 {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
			};

			DX::ThrowIfFailed(m_pDevice->CreateInputLayout(desc, _countof(desc), pBlobVSCube->GetBufferPointer(), pBlobVSCube->GetBufferSize(), m_pRenderCubeInputLayout.ReleaseAndGetAddressOf()));
		}

		{
						
			D3D11_RASTERIZER_DESC  desc = {};
			desc.FillMode = D3D11_FILL_WIREFRAME;
			desc.CullMode = D3D11_CULL_NONE;			
			DX::ThrowIfFailed(m_pDevice->CreateRasterizerState(&desc, m_pRenderCubeRasterState.ReleaseAndGetAddressOf()));
		}
	}

	auto InitializeRenderTextures() -> void {

		Microsoft::WRL::ComPtr<ID3D11Texture2D> pTexturePosition;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureNormal;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureColor;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> pTexturePositionSum;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureNormalSum;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureColorSum;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureToneMap;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureDenoiser;
	
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


			DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTexturePosition.ReleaseAndGetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureNormal.ReleaseAndGetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureColor.ReleaseAndGetAddressOf()));

			DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTexturePositionSum.ReleaseAndGetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureNormalSum.ReleaseAndGetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureColorSum.ReleaseAndGetAddressOf()));

		
			DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTexturePosition.Get(), nullptr, m_pSRVPosition.ReleaseAndGetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureNormal.Get(), nullptr, m_pSRVNormal.ReleaseAndGetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureColor.Get(), nullptr, m_pSRVColor.ReleaseAndGetAddressOf()));

			DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTexturePositionSum.Get(), nullptr, m_pSRVPositionSum.ReleaseAndGetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureNormalSum.Get(), nullptr, m_pSRVNormalSum.ReleaseAndGetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureColorSum.Get(), nullptr, m_pSRVColorSum.ReleaseAndGetAddressOf()));
		
			DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTexturePosition.Get(), nullptr, m_pUAVPosition.ReleaseAndGetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureNormal.Get(), nullptr,   m_pUAVNormal.ReleaseAndGetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureColor.Get(), nullptr, m_pUAVColor.ReleaseAndGetAddressOf()));

			
			DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTexturePositionSum.Get(), nullptr, m_pUAVPositionSum.ReleaseAndGetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureNormalSum.Get(), nullptr, m_pUAVNormalSum.ReleaseAndGetAddressOf()));
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
			DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureDenoiser.GetAddressOf()));

			DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureToneMap.Get(), nullptr, m_pSRVToneMap.GetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureDenoiser.Get(), nullptr, m_pSRVDenoiser.GetAddressOf()));

			DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureToneMap.Get(), nullptr, m_pUAVToneMap.GetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureDenoiser.Get(), nullptr, m_pUAVDenoiser.GetAddressOf()));

		}


	}

	auto InitializeConstantBuffers() -> void {
		 m_pCBFrame = DX::CreateConstantBuffer<FrameBuffer>(m_pDevice);
	}

	auto InitializeEnviromentMap() -> void {
		 DX::ThrowIfFailed(DirectX::CreateDDSTextureFromFile(m_pDevice.Get(), L"Data/Textures/qwantani_2k.dds", nullptr, m_pSRVEnviroment.GetAddressOf()));

	}

	auto Update(F32 deltaTime) -> void override {

		try {
			if (m_IsReloadShader) {
				InitializeShaders();
				m_IsReloadShader = false;
			}
			
			if (m_IsReloadTranferFunc) {
				InitializeTransferFunction();
				m_IsReloadTranferFunc = false;
			}
		}
		catch (std::exception const& e) {
			std::cout << e.what() << std::endl;
		}

		auto view = m_Camera.ToMatrix();
		auto proj = Hawk::Math::Perspective(Hawk::Math::Radians(m_FieldOfView), m_ApplicationDesc.Width / static_cast<F32>(m_ApplicationDesc.Height), 0.1f, 1000.0f);

	
		DX::MapHelper<FrameBuffer> map(m_pImmediateContext, m_pCBFrame, D3D11_MAP_WRITE_DISCARD, 0);
		map->InvViewProjection = Hawk::Math::Transpose(Hawk::Math::Inverse(proj * view));	
		map->BoundingBoxMin  = m_BoundingBoxMin;
		map->BoundingBoxMax  = m_BoundingBoxMax;
		map->BoundingBoxSize = m_BoundingBoxSize;
		map->BoundingBoxRotation =
			Hawk::Math::RotateX(Hawk::Math::Radians(m_BoundingRotation.x)) *
			Hawk::Math::RotateY(Hawk::Math::Radians(m_BoundingRotation.y)) *
			Hawk::Math::RotateZ(Hawk::Math::Radians(m_BoundingRotation.z));

		map->Density = m_Density;
		map->StepCount = m_StepCount;
		map->TraceDepth = m_TraceDepth;
		map->FrameIndex = m_FrameIndex;
		map->Exposure = m_Exposure;
		map->GradientDelta = Hawk::Math::Vec3(1.0f / m_DimensionX, 1.0f / m_DimensionY, 1.0f / m_DimensionZ);
		map->FrameOffset = Hawk::Math::Vec2(2.0f * m_RandomDistribution(m_RandomGenerator) - 1.0f, 2.0f * m_RandomDistribution(m_RandomGenerator) - 1.0f);
		map->Gamma = m_Gamma;
		map->DenoiserStrange = m_DenoiserStrange;
		map->IBLScale = m_IBLScale;
		map->IsEnableEnviroment = m_IsEnableEnviroment ? 1.0f : -1.0f;


		if (auto point = std::make_tuple(0, 0); SDL_GetRelativeMouseState(&std::get<0>(point), &std::get<1>(point)) & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
			m_Camera.Rotate(Hawk::Components::Camera::LocalUp, -m_RotateSensivity * deltaTime * static_cast<F32>(std::get<0>(point)));
			m_Camera.Rotate(m_Camera.Right(), -m_RotateSensivity * deltaTime * static_cast<F32>(std::get<1>(point)));
			m_FrameIndex = 0;
		}

		{
			auto pKeyboardState = SDL_GetKeyboardState(nullptr);

			auto distance = Hawk::Math::Vec3{ 0.0f, 0.0f, 0.0f };

			if ((pKeyboardState[SDL_SCANCODE_DOWN]) || (pKeyboardState[SDL_SCANCODE_W]))
				distance += deltaTime * m_MoveSensivity * m_Camera.Forward();

			if ((pKeyboardState[SDL_SCANCODE_DOWN]) || (pKeyboardState[SDL_SCANCODE_S]))
				distance -= deltaTime * m_MoveSensivity * m_Camera.Forward();

			if ((pKeyboardState[SDL_SCANCODE_DOWN]) || (pKeyboardState[SDL_SCANCODE_A]))
				distance -= deltaTime * m_MoveSensivity * m_Camera.Right();

			if ((pKeyboardState[SDL_SCANCODE_DOWN]) || (pKeyboardState[SDL_SCANCODE_D]))
				distance += deltaTime * m_MoveSensivity * m_Camera.Right();

			if (distance != Hawk::Math::Vec3(0.0f, 0.0f, 0.0f))
				m_FrameIndex = 0;


			m_Camera.Translate(distance);
		}

	}

	auto Blit(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pSrc, Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pDst) -> void {

		ID3D11ShaderResourceView* ppSRVClear[] = {
			nullptr
		};
		
		m_pImmediateContext->OMSetRenderTargets(1, pDst.GetAddressOf(), nullptr);
		m_pImmediateContext->RSSetViewports(1, &CD3D11_VIEWPORT{ 0.0f, 0.0f, static_cast<float>(m_ApplicationDesc.Width), static_cast<float>(m_ApplicationDesc.Height), 0.0f, 1.0f });

		m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pImmediateContext->IASetInputLayout(nullptr);
		m_pImmediateContext->VSSetShader(m_pVSBlit.Get(), nullptr, 0);
		m_pImmediateContext->PSSetShader(m_pPSBlit.Get(), nullptr, 0);
		m_pImmediateContext->PSSetShaderResources(0, 1, pSrc.GetAddressOf());
		m_pImmediateContext->PSSetSamplers(0, 1, m_pSamplerPoint.GetAddressOf());
		m_pImmediateContext->Draw(6, 0);



	


		m_pImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);
		m_pImmediateContext->RSSetViewports(0, nullptr);

		m_pImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED);
		m_pImmediateContext->IASetInputLayout(nullptr);
		m_pImmediateContext->RSSetState(nullptr);
		m_pImmediateContext->VSSetShader(nullptr, nullptr, 0);
		m_pImmediateContext->PSSetShader(nullptr, nullptr, 0);
		m_pImmediateContext->PSSetSamplers(0, 0, nullptr);
		m_pImmediateContext->PSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
		


	}

	auto RenderCube(Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRTV) -> void {

		ID3D11ShaderResourceView* ppSRVClear[] = {
			nullptr
		};

		m_pImmediateContext->OMSetRenderTargets(1, pRTV.GetAddressOf(), nullptr);
		m_pImmediateContext->RSSetViewports(1, &CD3D11_VIEWPORT{ 0.0f, 0.0f, static_cast<float>(m_ApplicationDesc.Width), static_cast<float>(m_ApplicationDesc.Height), 0.0f, 1.0f });

		m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pImmediateContext->IASetInputLayout(m_pRenderCubeInputLayout.Get());
		m_pImmediateContext->VSSetShader(m_pVSRenderCube.Get(), nullptr, 0);
		m_pImmediateContext->PSSetShader(m_pPSRenderCube.Get(), nullptr, 0);
		m_pImmediateContext->VSSetConstantBuffers(0, 1, m_pCBFrame.GetAddressOf());
		m_pImmediateContext->RSSetState(m_pRenderCubeRasterState.Get());
		m_pCube->Render(m_pImmediateContext);

		m_pImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);
		m_pImmediateContext->RSSetViewports(0, nullptr);

		m_pImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED);
		m_pImmediateContext->IASetInputLayout(nullptr);
		m_pImmediateContext->RSSetState(nullptr);
		m_pImmediateContext->VSSetShader(nullptr, nullptr, 0);
		m_pImmediateContext->PSSetShader(nullptr, nullptr, 0);
		m_pImmediateContext->PSSetSamplers(0, 0, nullptr);
		m_pImmediateContext->PSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);

	}

	auto RenderFrame(Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRTV) -> void override {
	
	

	

		ID3D11UnorderedAccessView* ppUAVClear[] = {
			nullptr,
			nullptr,
			nullptr
		};

		ID3D11ShaderResourceView* ppSRVClear [] = {
			nullptr,
			nullptr,
			nullptr
		};


		auto threadGroupsX = static_cast<uint32_t>(std::ceil(m_ApplicationDesc.Width  / 8.0f));
		auto threadGroupsY = static_cast<uint32_t>(std::ceil(m_ApplicationDesc.Height / 8.0f));

	
		{

			ID3D11SamplerState* ppSamplers[] = {
				 m_pSamplerPoint.Get(),
				 m_pSamplerLinear.Get(),
				 m_pSamplerAnisotropic.Get()
			};

			ID3D11ShaderResourceView* ppSRVTextures[] = {
				m_pSRVVolumeIntensity.Get(),
				m_pSRVVolumeGradient.Get(),
				m_pSRVDiffuseTF.Get(),
				m_pSRVSpecularTF.Get(),
				m_pSRVRoughnessTF.Get(),
				m_pSRVOpasityTF.Get(),
				m_pSRVEnviroment.Get()
			};


			ID3D11UnorderedAccessView* ppUAVTextures[] = {
				m_pUAVPosition.Get(),
				m_pUAVNormal.Get(),
				m_pUAVColor.Get(),
			
			};

			//First pass Sample Monte Carlo
			m_pImmediateContext->CSSetShader(m_pCSPathTracingFirstPass.Get(), nullptr, 0);
			m_pImmediateContext->CSSetConstantBuffers(0, 1, m_pCBFrame.GetAddressOf());
			m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplers), ppSamplers);
			m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVTextures), ppSRVTextures);
			m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVTextures), ppUAVTextures, nullptr);
			m_pImmediateContext->Dispatch(threadGroupsX, threadGroupsY, 1);
			m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
			m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
		}

		{

			ID3D11ShaderResourceView* ppSRVTextures[] = {
				m_pSRVPosition.Get(),
				m_pSRVNormal.Get(),
				m_pSRVColor.Get()
			};


			ID3D11UnorderedAccessView* ppUAVTextures[] = {
				m_pUAVPositionSum.Get(),
				m_pUAVNormalSum.Get(),
				m_pUAVColorSum.Get()
			};


			//Second pass Integrate Monte Carlo
			m_pImmediateContext->CSSetShader(m_pCSPathTracingSecondPass.Get(), nullptr, 0);
			m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVTextures), ppSRVTextures);
			m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVTextures), ppUAVTextures, nullptr);
			m_pImmediateContext->Dispatch(threadGroupsX, threadGroupsY, 1);
			m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
			m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
		}

		{

			ID3D11ShaderResourceView* ppSRVTextures[] = {
				m_pSRVColorSum.Get()
			};


			ID3D11UnorderedAccessView* ppUAVTextures[] = {
				m_pUAVToneMap.Get()
			};

			//Tone map
			m_pImmediateContext->CSSetShader(m_pCSToneMap.Get(), nullptr, 0);
			m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVTextures), ppSRVTextures);
			m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVTextures), ppUAVTextures, nullptr);
			m_pImmediateContext->Dispatch(threadGroupsX, threadGroupsY, 1);
			m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
			m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);

		}
	
		//Denoiser
		{

			ID3D11ShaderResourceView* ppSRVTextures[] = {
				m_pSRVPositionSum.Get(),
				m_pSRVNormalSum.Get(),
				m_pSRVToneMap.Get(),
			};


			ID3D11UnorderedAccessView* ppUAVTextures[] = {
				m_pUAVDenoiser.Get()
			};

			m_pImmediateContext->CSSetShader(m_pCSDenoiser.Get(), nullptr, 0);
			m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVTextures), ppSRVTextures);
			m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVTextures), ppUAVTextures, nullptr);
			m_pImmediateContext->Dispatch(threadGroupsX, threadGroupsY, 1);
			m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
			m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);

		}
	
	
		this->Blit(m_pSRVDenoiser, pRTV);
		

		if (m_IsRenderCube) 
			this->RenderCube(pRTV);
		
		m_FrameIndex++;

	
		

	}

	auto RenderGUI(Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRTV) -> void override {

		
		ImGui::Begin("Debug");
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		m_FrameIndex = ImGui::Checkbox("Reload shader", &m_IsReloadShader) ? 0 : m_FrameIndex;
		m_FrameIndex = ImGui::Checkbox("Reload tranfer func", &m_IsReloadTranferFunc) ? 0 : m_FrameIndex;
		ImGui::End();

		ImGui::Begin("Settings");
		m_FrameIndex = ImGui::DragFloat3("Bounding rotation", std::data(m_BoundingRotation)) ? 0 : m_FrameIndex;
		ImGui::NewLine();
		ImGui::SliderFloat("Rotate sentivity", &m_RotateSensivity, 0.1f, 10.0f);
		ImGui::SliderFloat("Move sentivity", &m_MoveSensivity, 0.1f, 10.0f);
		ImGui::NewLine();
		m_FrameIndex = ImGui::SliderFloat("Density", &m_Density, 0.1f, 100.0f) ? 0 : m_FrameIndex;
		ImGui::SliderFloat("Denoiser strange", &m_DenoiserStrange, 0.0f, 3.0f);
		m_FrameIndex = ImGui::SliderFloat("FieldOfView", &m_FieldOfView, 0.1f, 100.0f) ? 0 : m_FrameIndex;
		m_FrameIndex = ImGui::SliderInt("Step count", (int32_t*)&m_StepCount, 1, 512) ? 0 : m_FrameIndex;
		m_FrameIndex = ImGui::SliderInt("Trace depth", (int32_t*)&m_TraceDepth, 1, 10) ? 0 : m_FrameIndex;
		ImGui::NewLine();

		
		
		ImGui::PlotLines("Opacity",
			[](void* data, int idx) { return static_cast<ScalarTransferFunction1D*>(data)->Evaluate(idx / static_cast<F32>(64)); },
			&m_OpacityTransferFunc, 64);

		ImGui::NewLine();
		ImGui::SliderFloat("Gamma", &m_Gamma, 1.8f, 3.0f);
		ImGui::SliderFloat("Exposure", &m_Exposure, 0.1f, 6.0f);
		m_FrameIndex = ImGui::SliderFloat("IBLScale", &m_IBLScale, 0.1f, 10.0f) ? 0 : m_FrameIndex;
		m_FrameIndex = ImGui::Checkbox("Enable enviroment", &m_IsEnableEnviroment) ? 0 : m_FrameIndex;
		ImGui::Checkbox("Enable cube", &m_IsRenderCube);
		
		

		ImGui::End();
	

		ImGui::Begin("Cropping");
		m_FrameIndex = ImGui::SliderFloat("+X: ", &m_BoundingBoxMax.x, -m_BoundingBoxSize, +m_BoundingBoxSize) ? 0 : m_FrameIndex;
		m_FrameIndex = ImGui::SliderFloat("-X: ", &m_BoundingBoxMin.x, -m_BoundingBoxSize, +m_BoundingBoxSize) ? 0 : m_FrameIndex;
		m_FrameIndex = ImGui::SliderFloat("+Y: ", &m_BoundingBoxMax.y, -m_BoundingBoxSize, +m_BoundingBoxSize) ? 0 : m_FrameIndex;
		m_FrameIndex = ImGui::SliderFloat("-Y: ", &m_BoundingBoxMin.y, -m_BoundingBoxSize, +m_BoundingBoxSize) ? 0 : m_FrameIndex;
		m_FrameIndex = ImGui::SliderFloat("+Z: ", &m_BoundingBoxMax.z, -m_BoundingBoxSize, +m_BoundingBoxSize) ? 0 : m_FrameIndex;	
		m_FrameIndex = ImGui::SliderFloat("-Z: ", &m_BoundingBoxMin.z, -m_BoundingBoxSize, +m_BoundingBoxSize) ? 0 : m_FrameIndex;

		ImGui::End();


	}



private:
	
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVVolumeIntensity;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVVolumeGradient;
	
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVDiffuseTF;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVSpecularTF;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVRoughnessTF;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVOpasityTF;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVEnviroment;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pSRVPosition;
 	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pSRVNormal;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pSRVColor;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pSRVPositionSum;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pSRVNormalSum;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pSRVColorSum;

	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pUAVPosition;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pUAVNormal;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pUAVColor;	

	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pUAVPositionSum;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pUAVNormalSum;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pUAVColorSum;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pSRVToneMap;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pSRVDenoiser;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pUAVToneMap;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pUAVDenoiser;

	Microsoft::WRL::ComPtr<ID3D11VertexShader>    m_pVSBlit;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>     m_pPSBlit;
	Microsoft::WRL::ComPtr<ID3D11VertexShader>    m_pVSRenderCube;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>     m_pPSRenderCube;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>     m_pRenderCubeInputLayout;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_pRenderCubeRasterState;

	Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_pCSComputeGradient;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_pCSPathTracingFirstPass;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_pCSPathTracingSecondPass;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_pCSToneMap;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_pCSDenoiser;

	Microsoft::WRL::ComPtr<ID3D11SamplerState>  m_pSamplerPoint;
	Microsoft::WRL::ComPtr<ID3D11SamplerState>  m_pSamplerLinear;
	Microsoft::WRL::ComPtr<ID3D11SamplerState>  m_pSamplerAnisotropic;

	Microsoft::WRL::ComPtr<ID3D11Buffer>        m_pCBFrame;	
	std::shared_ptr<Cube>                       m_pCube;

	ColorTransferFunction1D                     m_DiffuseTransferFunc;
	ColorTransferFunction1D                     m_SpecularTransferFunc;
	ScalarTransferFunction1D                    m_RoughnessTransferFunc;
	ScalarTransferFunction1D                    m_OpacityTransferFunc;


	Hawk::Components::Camera  m_Camera;
	Hawk::Math::Vec3          m_BoundingRotation;
	Hawk::Math::Vec3          m_BoundingBoxMin = Hawk::Math::Vec3(-0.5f, -0.5f, -0.5f);
	Hawk::Math::Vec3          m_BoundingBoxMax = Hawk::Math::Vec3(+0.5f, +0.5f, +0.5f);

	F32      m_RotateSensivity = 0.25f;
	F32      m_MoveSensivity   = 0.5f;
	F32      m_BoundingBoxSize = 0.5f;
	F32      m_Density         = 30.0f;
	F32      m_Exposure        = 4.0f;
	F32      m_IBLScale        = 2.5f;
	F32      m_FieldOfView     = 60.0f;
	F32      m_Gamma           = 2.2f;
	F32      m_DenoiserStrange = 1.0f;
	uint32_t m_TraceDepth      = 2;
	uint32_t m_StepCount       = 255;
	uint32_t m_FrameIndex      = 0;
	bool     m_IsEnableEnviroment  = false;
	bool     m_IsRenderCube        = false;
	bool     m_IsReloadShader      = false;
	bool     m_IsReloadTranferFunc = false;
	

	uint16_t m_DimensionX = 0;
	uint16_t m_DimensionY = 0;
	uint16_t m_DimensionZ = 0;

	std::random_device                  m_RandomDevice;
	std::mt19937                        m_RandomGenerator;
	std::uniform_real_distribution<F32> m_RandomDistribution;
};
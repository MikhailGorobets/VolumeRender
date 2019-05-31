#pragma once

#include "Application.h"
#include "TransferFunction.h"



struct FrameBuffer {
	Hawk::Math::Mat4x4 CameraView;
	Hawk::Math::Mat4x4 BoundingBoxRotation;

	F32                CameraFocalLenght;
	Hawk::Math::Vec3   CameraOrigin;
	
	F32                CameraAspectRatio;
	uint32_t           FrameIndex;
	uint32_t           TraceDepth;
	uint32_t           StepCount;

	F32                Density;
	Hawk::Math::Vec3   BoundingBoxMin;

	F32                Exposure;
	Hawk::Math::Vec3   BoundingBoxMax;

	Hawk::Math::Vec3   GradientDelta;
	F32                GradFactor;

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
		//m_Camera.SetRotation(0.0f, 1.0f, 0.0f, Hawk::Math::PI<F32>);
		m_BoundingRotation = Hawk::Math::Vec3(-100.0f, 0.0f, 0.0f);
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
			intensity[index] = data[index] / static_cast<F32>(tmax);
		
		

		
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

		ColorTransferFunction1D  albedoTF;
		ScalarTransferFunction1D metallicTF;
		ScalarTransferFunction1D roughnessTF;
		ScalarTransferFunction1D opasityTF;

		
	
		albedoTF.AddNode(0.0f, { 0.627450f, 0.627450f, 0.6431372f });
		albedoTF.AddNode(0.23597f, { 0.176470f, 0.0f, 0.0f });
		albedoTF.AddNode(0.288538f, { 0.172549f, 0.02745f, 0.02745f });
		albedoTF.AddNode(0.288578f, { 0.242745f, 0.0f, 0.0f });	
		albedoTF.AddNode(0.35417f,  { 0.30725f, 0.28235f, 0.1607843f });
		albedoTF.AddNode(1.0f, { 0.62745f, 0.62745f, 0.643137f });

		metallicTF.AddNode(0.0f, 0.0f);
		metallicTF.AddNode(0.23597f, 0.0f);
		metallicTF.AddNode(0.288538f, 0.0);
		metallicTF.AddNode(0.288578f, 0.0f);
		metallicTF.AddNode(0.35417f, 0.0f);
		metallicTF.AddNode(1.0f, 0.00f);

		roughnessTF.AddNode(0.0f, 1.0f);
		roughnessTF.AddNode(0.23597f, 1.0f);
		roughnessTF.AddNode(0.288538f, 0.5868f);
		roughnessTF.AddNode(0.288578f, 0.563502f);
		roughnessTF.AddNode(0.35417f, 0.1322f);
		roughnessTF.AddNode(1.0f, 0.1322f);


		opasityTF.AddNode(0.0f, 0.0f);
		opasityTF.AddNode(0.23597f, 0.001f);
		opasityTF.AddNode(0.288538f, 0.179028f);
		opasityTF.AddNode(0.288578f, 0.6f);
		opasityTF.AddNode(0.35417f, 1.0f);
		opasityTF.AddNode(1.0f, 1.0f);
 
		m_pSRVAlbedoTF = albedoTF.GenerateTexture(m_pDevice);
		m_pSRVMetallicTF = metallicTF.GenerateTexture(m_pDevice);
		m_pSRVRoughnessTF = roughnessTF.GenerateTexture(m_pDevice);
		m_pSRVOpasityTF = opasityTF.GenerateTexture(m_pDevice);


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

			if (FAILED(D3DCompileFromFile(fileName, nullptr, nullptr, entrypoint, target, 0, 0, pCodeBlob.GetAddressOf(), pErrorBlob.GetAddressOf())))
				throw std::runtime_error(static_cast<const char*>(pErrorBlob->GetBufferPointer()));
			return pCodeBlob;
		};

		auto pBlobCSGradient = compileShader(L"Data/Shaders/ComputeGradient.hlsl", "ComputeGradient", "cs_5_0");
		auto pBlobCSFirstPass = compileShader(L"Data/Shaders/PathTracing.hlsl", "FirstPass", "cs_5_0");
		auto pBlobCSSecondPass = compileShader(L"Data/Shaders/PathTracing.hlsl", "SecondPass", "cs_5_0");
		auto pBlobCSToneMap = compileShader(L"Data/Shaders/ToneMap.hlsl", "ToneMap", "cs_5_0");
		auto pBlobVSBlit = compileShader(L"Data/Shaders/Blitting.hlsl", "FullQuad", "vs_5_0");
		auto pBlobPSBlit = compileShader(L"Data/Shaders/Blitting.hlsl", "Blitting", "ps_5_0");

		DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSGradient->GetBufferPointer(), pBlobCSGradient->GetBufferSize(), nullptr, m_pCSComputeGradient.GetAddressOf()));
		DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSFirstPass->GetBufferPointer(), pBlobCSFirstPass->GetBufferSize(), nullptr, m_pCSPathTracingFirstPass.GetAddressOf()));
		DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSSecondPass->GetBufferPointer(), pBlobCSSecondPass->GetBufferSize(), nullptr, m_pCSPathTracingSecondPass.GetAddressOf()));
		DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSToneMap->GetBufferPointer(), pBlobCSToneMap->GetBufferSize(), nullptr, m_pCSToneMap.GetAddressOf()));
		DX::ThrowIfFailed(m_pDevice->CreateVertexShader(pBlobVSBlit->GetBufferPointer(), pBlobVSBlit->GetBufferSize(), nullptr, m_pVSBlit.GetAddressOf()));
		DX::ThrowIfFailed(m_pDevice->CreatePixelShader(pBlobPSBlit->GetBufferPointer(), pBlobPSBlit->GetBufferSize(), nullptr, m_pPSBlit.GetAddressOf()));

	}

	auto InitializeRenderTextures() -> void {

		Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureFirstPass;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureSecondPass;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureToneMap;
		
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


			DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureFirstPass.GetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureSecondPass.GetAddressOf()));

			DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureFirstPass.Get(), nullptr, m_pSRVFirstPass.GetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureSecondPass.Get(), nullptr, m_pSRVSecondPass.GetAddressOf()));

			DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureFirstPass.Get(), nullptr, m_pUAVFirstPass.GetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureSecondPass.Get(), nullptr, m_pUAVSecondPass.GetAddressOf()));
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

	auto InitializeConstantBuffers() -> void {
		 m_pCBFrame = DX::CreateConstantBuffer<FrameBuffer>(m_pDevice);
	}

	auto InitializeEnviromentMap() -> void {
		 DX::ThrowIfFailed(DirectX::CreateDDSTextureFromFile(m_pDevice.Get(), L"Data/Textures/waterbuck_trail_4k.dds", nullptr, m_pSRVEnviroment.GetAddressOf()));

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
		m_pImmediateContext->VSSetShader(nullptr, nullptr, 0);
		m_pImmediateContext->PSSetShader(nullptr, nullptr, 0);
		m_pImmediateContext->PSSetSamplers(0, 0, nullptr);
		m_pImmediateContext->PSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
		


	}



	auto Update(F32 deltaTime) -> void override {
		

		DX::MapHelper<FrameBuffer> map(m_pImmediateContext, m_pCBFrame, D3D11_MAP_WRITE_DISCARD, 0);
		map->CameraOrigin = m_Camera.Translation();
		map->CameraView = Hawk::Math::Transpose(Hawk::Math::Convert<Hawk::Math::Quat, Hawk::Math::Mat4x4>(m_Camera.Rotation()));
		map->CameraAspectRatio = m_ApplicationDesc.Width / static_cast<F32>(m_ApplicationDesc.Height);
		map->CameraFocalLenght = 1.0f / Hawk::Math::Tan(Hawk::Math::Radians(m_FieldOfView / 2.0f));

		map->BoundingBoxMin = Hawk::Math::Vec3(-m_BoundingSize, -m_BoundingSize, -m_BoundingSize);
		map->BoundingBoxMax = Hawk::Math::Vec3( m_BoundingSize,  m_BoundingSize,  m_BoundingSize);
		map->BoundingBoxRotation = 
			Hawk::Math::RotateX(Hawk::Math::Radians(m_BoundingRotation.x)) * 
			Hawk::Math::RotateY(Hawk::Math::Radians(m_BoundingRotation.y)) * 
			Hawk::Math::RotateZ(Hawk::Math::Radians(m_BoundingRotation.z));
		map->Density   = m_Density;
		map->StepCount = m_StepCount;
		map->TraceDepth = m_TraceDepth;
		map->FrameIndex = m_FrameIndex;
		map->Exposure = m_Exposure;
		map->GradientDelta = Hawk::Math::Vec3(1.0f / m_DimensionX, 1.0f / m_DimensionY, 1.0f / m_DimensionZ);
		map->FrameOffset = Hawk::Math::Vec2(2.0f * m_RandomDistribution(m_RandomGenerator) - 1.0f, 2.0f * m_RandomDistribution(m_RandomGenerator) - 1.0f);
		map->Gamma = m_Gamma;
		map->GradFactor = m_GradFactor;
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

	auto RenderFrame(Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRTV) -> void override {
	
	
		ID3D11SamplerState* ppSamplers[] = {
			 m_pSamplerPoint.Get(),
			 m_pSamplerLinear.Get(),
			 m_pSamplerAnisotropic.Get()
		};

		ID3D11ShaderResourceView* ppTextures[] = {
			m_pSRVVolumeIntensity.Get(),
			m_pSRVVolumeGradient.Get(),
			m_pSRVAlbedoTF.Get(),
			m_pSRVMetallicTF.Get(),
			m_pSRVRoughnessTF.Get(),
			m_pSRVOpasityTF.Get(),
			m_pSRVEnviroment.Get()
		};

		ID3D11UnorderedAccessView* ppUAVClear[] = {
			nullptr
		};

		ID3D11ShaderResourceView* ppSRVClear [] = {
			nullptr
		};


		auto threadGroupsX = static_cast<uint32_t>(std::ceil(m_ApplicationDesc.Width  / 8.0f));
		auto threadGroupsY = static_cast<uint32_t>(std::ceil(m_ApplicationDesc.Height / 8.0f));

		

		
		
		
		//First pass Sample Monte Carlo
		m_pImmediateContext->CSSetShader(m_pCSPathTracingFirstPass.Get(), nullptr, 0);
		m_pImmediateContext->CSSetConstantBuffers(0, 1, m_pCBFrame.GetAddressOf());
		m_pImmediateContext->CSSetSamplers(0, _countof(ppSamplers), ppSamplers);
		m_pImmediateContext->CSSetShaderResources(0, _countof(ppTextures), ppTextures);
		m_pImmediateContext->CSSetUnorderedAccessViews(0, 1, m_pUAVFirstPass.GetAddressOf(), nullptr);
		m_pImmediateContext->Dispatch(threadGroupsX, threadGroupsY, 1);
		m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
		m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);

		//Second pass Integrate Monte Carlo
		m_pImmediateContext->CSSetShader(m_pCSPathTracingSecondPass.Get(), nullptr, 0);
		m_pImmediateContext->CSSetShaderResources(0, 1, m_pSRVFirstPass.GetAddressOf());
		m_pImmediateContext->CSSetUnorderedAccessViews(0, 1, m_pUAVSecondPass.GetAddressOf(), nullptr);
		m_pImmediateContext->Dispatch(threadGroupsX, threadGroupsY, 1);
		m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
		m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
	
		//Tone map
		m_pImmediateContext->CSSetShader(m_pCSToneMap.Get(), nullptr, 0);
		m_pImmediateContext->CSSetShaderResources(0, 1, m_pSRVSecondPass.GetAddressOf());
		m_pImmediateContext->CSSetUnorderedAccessViews(0, 1, m_pUAVToneMap.GetAddressOf(), reinterpret_cast<uint32_t*>(m_pUAVToneMap.GetAddressOf()));
		m_pImmediateContext->Dispatch(threadGroupsX, threadGroupsY, 1);
		m_pImmediateContext->CSSetUnorderedAccessViews(0, _countof(ppUAVClear), ppUAVClear, nullptr);
		m_pImmediateContext->CSSetShaderResources(0, _countof(ppSRVClear), ppSRVClear);
	
	
		this->Blit(m_pSRVToneMap, pRTV);

		m_FrameIndex++;

	
		

	}

	auto RenderGUI(Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRTV) -> void override {

	
		
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

		ImGui::Begin("Settings");
		m_FrameIndex = ImGui::SliderFloat("Bounding size", &m_BoundingSize, 0.1f, 10.0f) ? 0 : m_FrameIndex;
		m_FrameIndex = ImGui::DragFloat3("Bounding rotation", std::data(m_BoundingRotation)) ? 0 : m_FrameIndex;
		ImGui::NewLine();
		ImGui::SliderFloat("Rotate sentivity", &m_RotateSensivity, 0.1f, 10.0f);
		ImGui::SliderFloat("Move sentivity", &m_MoveSensivity, 0.1f, 10.0f);
		ImGui::NewLine();
		m_FrameIndex = ImGui::SliderFloat("Density", &m_Density, 0.1f, 100.0f) ? 0 : m_FrameIndex;
		m_FrameIndex = ImGui::SliderFloat("Grad factor", &m_GradFactor, 0.1f, 100.0f) ? 0 : m_FrameIndex;
		m_FrameIndex = ImGui::SliderFloat("FieldOfView", &m_FieldOfView, 0.1f, 100.0f) ? 0 : m_FrameIndex;
		m_FrameIndex = ImGui::SliderInt("Step count", (int32_t*)&m_StepCount, 1, 512) ? 0 : m_FrameIndex;
		m_FrameIndex = ImGui::SliderInt("Trace depth", (int32_t*)&m_TraceDepth, 1, 10) ? 0 : m_FrameIndex;
		ImGui::NewLine();
		ImGui::SliderFloat("Gamma", &m_Gamma, 1.8f, 3.0f);
		ImGui::SliderFloat("Exposure", &m_Exposure, 0.1f, 6.0f);
		m_FrameIndex = ImGui::Checkbox("Enable enviroment", &m_IsEnableEnviroment) ? 0 : m_FrameIndex;
		
		ImGui::End();
	

	}



private:
	
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVVolumeIntensity;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVVolumeGradient;
	
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVAlbedoTF;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVMetallicTF;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVRoughnessTF;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVOpasityTF;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVEnviroment;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pSRVFirstPass;	
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pSRVSecondPass;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pSRVToneMap;

	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pUAVFirstPass;	
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pUAVSecondPass;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pUAVToneMap;

	Microsoft::WRL::ComPtr<ID3D11VertexShader>  m_pVSBlit;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>   m_pPSBlit;

	Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_pCSComputeGradient;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_pCSPathTracingFirstPass;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_pCSPathTracingSecondPass;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_pCSToneMap;

	Microsoft::WRL::ComPtr<ID3D11SamplerState>  m_pSamplerPoint;
	Microsoft::WRL::ComPtr<ID3D11SamplerState>  m_pSamplerLinear;
	Microsoft::WRL::ComPtr<ID3D11SamplerState>  m_pSamplerAnisotropic;

	Microsoft::WRL::ComPtr<ID3D11Buffer>        m_pCBFrame;

	Hawk::Components::Camera  m_Camera;
	Hawk::Math::Vec3          m_BoundingRotation;

	F32      m_RotateSensivity = 0.25f;
	F32      m_MoveSensivity   = 0.5f;
	F32      m_BoundingSize    = 0.5f;
	F32      m_Density         = 30.0f;
	F32      m_Exposure        = 4.0f;
	F32      m_FieldOfView     = 60.0f;
	F32      m_Gamma           = 2.2f;
	F32      m_GradFactor      = 4.0f;
	uint32_t m_TraceDepth      = 2;
	uint32_t m_StepCount       = 255;
	uint32_t m_FrameIndex      = 0;
	bool     m_IsEnableEnviroment = false;

	uint16_t m_DimensionX = 0;
	uint16_t m_DimensionY = 0;
	uint16_t m_DimensionZ = 0;

	std::random_device                  m_RandomDevice;
	std::mt19937                        m_RandomGenerator;
	std::uniform_real_distribution<F32> m_RandomDistribution;
};
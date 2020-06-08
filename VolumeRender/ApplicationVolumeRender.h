#pragma once

#include "Application.h"
#include "TransferFunction.h"


struct FrameBuffer {
	Hawk::Math::Mat4x4 ViewProjectionMatrix;
	Hawk::Math::Mat4x4 WorldMatrix;
	Hawk::Math::Mat4x4 NormalMatrix;

	Hawk::Math::Mat4x4 InvViewProjectionMatrix;
	Hawk::Math::Mat4x4 InvWorldMatrix;
	Hawk::Math::Mat4x4 InvNormalMatrix;

	F32                DenoiserStrange;
	uint32_t           FrameIndex;
	uint32_t           TraceDepth;
	uint32_t           StepCount;

	F32                Density;
	Hawk::Math::Vec3   BoundingBoxMin;

	F32                Exposure;
	Hawk::Math::Vec3   BoundingBoxMax;

	Hawk::Math::Vec2   FrameOffset;
	Hawk::Math::Vec2   RenderTargetDim;


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
	
		

	}

private:

	auto InitializeVolumeTexture() -> void {

		
		std::ifstream file("Data/Textures/manix.dat", std::ifstream::binary);
		if (!file.is_open())
			throw std::runtime_error("Failed to open file: " + std::string("Data/Textures/manix.dat"));

		
		file.read(reinterpret_cast<char*>(&m_DimensionX), sizeof(uint16_t));
		file.read(reinterpret_cast<char*>(&m_DimensionY), sizeof(uint16_t));
		file.read(reinterpret_cast<char*>(&m_DimensionZ), sizeof(uint16_t));

		std::vector<uint16_t> data(size_t(m_DimensionX) * size_t(m_DimensionY) * size_t(m_DimensionZ));
		std::vector<F32> intensity(size_t(m_DimensionX) * size_t(m_DimensionY) * size_t(m_DimensionZ));

		uint16_t tmin = (std::numeric_limits<uint16_t>::max)();
		uint16_t tmax = (std::numeric_limits<uint16_t>::min)();

	
		auto t1 = std::chrono::high_resolution_clock::now();
		for (auto index = 0u; index < std::size(data); index++) {
			file.read(reinterpret_cast<char*>(&data[index]), sizeof(uint16_t));
 			tmin = (std::min)(tmin, data[index]);
			tmax = (std::max)(tmax, data[index]);
		}

		auto t2 = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

		for (auto index = 0u; index < std::size(intensity); index++)
			intensity[index] = (data[index] - tmin) / static_cast<F32>(tmax - tmin);
		

		{

			D3D11_TEXTURE3D_DESC desc = {};
			desc.Width = m_DimensionX;
			desc.Height = m_DimensionY;
			desc.Depth = m_DimensionZ;
			desc.Format = DXGI_FORMAT_R32_FLOAT;
			desc.MipLevels = 1;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			desc.Usage = D3D11_USAGE_DEFAULT;


			D3D11_SUBRESOURCE_DATA resourceData = {};
			resourceData.pSysMem = std::data(intensity);
			resourceData.SysMemPitch = sizeof(F32) * desc.Width;
			resourceData.SysMemSlicePitch = sizeof(F32) * desc.Height * desc.Width;


			Microsoft::WRL::ComPtr<ID3D11Texture3D> pTextureIntensity;
			DX::ThrowIfFailed(m_pDevice->CreateTexture3D(&desc, &resourceData, pTextureIntensity.GetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureIntensity.Get(), nullptr, m_pSRVVolumeIntensity.GetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureIntensity.Get(), nullptr, m_pUAVVolumeIntensity.GetAddressOf()));

		}	

	
	}

	auto InitializeTransferFunction() -> void {
	
		boost::property_tree::ptree root;
		boost::property_tree::read_json("Data/TransferFunctions/ManixTransferFunction.json", root);
		
		m_OpacityTransferFunc.Clear();
		m_DiffuseTransferFunc.Clear();
		m_SpecularTransferFunc.Clear();
		m_EmissionTransferFunc.Clear();
		m_RoughnessTransferFunc.Clear();

		auto extractFunction = [](auto const& tree, auto const& key) -> Hawk::Math::Vec3 {
			Hawk::Math::Vec3 v{};
			auto index = 0;
			for (auto& e : tree.get_child(key)) {
				v[index] = e.second.get_value<F32>();
				index++;
			}
			return v;
			
		};

		for (auto const& e : root.get_child("NodesColor")) {

			auto intensity = e.second.get<F32>("Intensity");	
			auto diffuse    = extractFunction(e.second, "Diffuse");
			auto specular   = extractFunction(e.second, "Specular");
			auto roughness  = e.second.get<F32>("Roughness");

			m_DiffuseTransferFunc.AddNode(intensity, diffuse);
			m_SpecularTransferFunc.AddNode(intensity, specular);
			m_RoughnessTransferFunc.AddNode(intensity, roughness);			
		}


		for (auto const& e : root.get_child("NodesOpacity")) {

			auto intensity = e.second.get<F32>("Intensity");
			auto opacity = e.second.get<F32>("Opacity");
			m_OpacityTransferFunc.AddNode(intensity, opacity);

		}

		m_pSRVOpasityTF   = m_OpacityTransferFunc.GenerateTexture(m_pDevice, m_SamplingCount);
		m_pSRVDiffuseTF   = m_DiffuseTransferFunc.GenerateTexture(m_pDevice, m_SamplingCount);
		m_pSRVSpecularTF  = m_SpecularTransferFunc.GenerateTexture(m_pDevice, m_SamplingCount);
		m_pSRVRoughnessTF = m_RoughnessTransferFunc.GenerateTexture(m_pDevice, m_SamplingCount);
		


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

		auto pBlobCSFirstPass = compileShader(L"Data/Shaders/PathTracing.hlsl", "RayTracing", "cs_5_0");
		auto pBlobCSSecondPass = compileShader(L"Data/Shaders/PathTracing.hlsl", "FrameSum", "cs_5_0");
		auto pBlobCSToneMap = compileShader(L"Data/Shaders/ToneMap.hlsl", "ToneMap", "cs_5_0");
		auto pBlobVSBlit = compileShader(L"Data/Shaders/Blitting.hlsl", "BlitVS", "vs_5_0");
		auto pBlobPSBlit = compileShader(L"Data/Shaders/Blitting.hlsl", "BlitPS", "ps_5_0");		
	



		DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSFirstPass->GetBufferPointer(), pBlobCSFirstPass->GetBufferSize(), nullptr, m_pCSPathTracingFirstPass.ReleaseAndGetAddressOf()));
		DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSSecondPass->GetBufferPointer(), pBlobCSSecondPass->GetBufferSize(), nullptr, m_pCSPathTracingSecondPass.ReleaseAndGetAddressOf()));
		DX::ThrowIfFailed(m_pDevice->CreateComputeShader(pBlobCSToneMap->GetBufferPointer(), pBlobCSToneMap->GetBufferSize(), nullptr, m_pCSToneMap.ReleaseAndGetAddressOf()));

		DX::ThrowIfFailed(m_pDevice->CreateVertexShader(pBlobVSBlit->GetBufferPointer(), pBlobVSBlit->GetBufferSize(), nullptr, m_pVSBlit.ReleaseAndGetAddressOf()));
		DX::ThrowIfFailed(m_pDevice->CreatePixelShader(pBlobPSBlit->GetBufferPointer(), pBlobPSBlit->GetBufferSize(), nullptr, m_pPSBlit.ReleaseAndGetAddressOf()));
	}

	auto InitializeRenderTextures() -> void {


		Microsoft::WRL::ComPtr<ID3D11Texture2D> pTextureColor;
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

			DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureColor.ReleaseAndGetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateTexture2D(&desc, nullptr, pTextureColorSum.ReleaseAndGetAddressOf()));

			DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureColor.Get(), nullptr, m_pSRVColor.ReleaseAndGetAddressOf()));
			DX::ThrowIfFailed(m_pDevice->CreateShaderResourceView(pTextureColorSum.Get(), nullptr, m_pSRVColorSum.ReleaseAndGetAddressOf()));

			DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureColor.Get(), nullptr, m_pUAVColor.ReleaseAndGetAddressOf()));
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
			DX::ThrowIfFailed(m_pDevice->CreateUnorderedAccessView(pTextureToneMap.Get(), nullptr, m_pUAVToneMap.GetAddressOf()));

		}


	}

	auto InitializeConstantBuffers() -> void {
		 m_pCBFrame = DX::CreateConstantBuffer<FrameBuffer>(m_pDevice);
	}

	auto InitializeEnviromentMap() -> void {
		 DX::ThrowIfFailed(DirectX::CreateDDSTextureFromFile(m_pDevice.Get(), L"Data/Textures/qwantani_2k.dds", nullptr, m_pSRVEnviroment.GetAddressOf()));

	}

	auto EventMouseWheel(F32 delta) -> void override {
		m_Zoom -= m_ZoomSensivity * m_DeltaTime * delta;
		m_FrameIndex = 0;
	}


	auto EventMouseMove(F32 x, F32 y) ->  void override {

		 m_Camera.Rotate(Hawk::Components::Camera::LocalUp, m_DeltaTime * -m_RotateSensivity * x);
		 m_Camera.Rotate(m_Camera.Right(), m_DeltaTime * -m_RotateSensivity  * y);
		 m_FrameIndex = 0;
	}

	auto Update(F32 deltaTime) -> void override {

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
		}
		catch (std::exception const& e) {
			std::cout << e.what() << std::endl;
		}
		
		auto scaleVector = Hawk::Math::Vec3(0.488f * m_DimensionX, 0.488f * m_DimensionY, 0.7f * m_DimensionZ);
		scaleVector /= (std::max)({scaleVector.x, scaleVector.y, scaleVector.z});


		auto view  = m_Camera.ToMatrix();	
		auto proj  = Hawk::Math::Orthographic(m_Zoom * (m_ApplicationDesc.Width / static_cast<F32>(m_ApplicationDesc.Height)), m_Zoom, -1.0f, 10.0f);
		auto world = Hawk::Math::RotateX(Hawk::Math::Radians(-90.0f));
		auto normal = Hawk::Math::Inverse(Hawk::Math::Transpose(world));


		{
			DX::MapHelper<FrameBuffer> map(m_pImmediateContext, m_pCBFrame, D3D11_MAP_WRITE_DISCARD, 0);


			map->BoundingBoxMin = scaleVector * m_BoundingBoxMin;
			map->BoundingBoxMax = scaleVector * m_BoundingBoxMax;
			map->ViewProjectionMatrix = proj * view;
			map->WorldMatrix = world;
			map->NormalMatrix = normal;

			map->InvViewProjectionMatrix = Hawk::Math::Inverse(map->ViewProjectionMatrix);
			map->InvWorldMatrix    = Hawk::Math::Inverse(map->WorldMatrix);
			map->InvNormalMatrix   = Hawk::Math::Inverse(map->NormalMatrix);

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
				m_pSRVDiffuseTF.Get(),
				m_pSRVSpecularTF.Get(),
				m_pSRVRoughnessTF.Get(),
				m_pSRVOpasityTF.Get(),
				m_pSRVEnviroment.Get()
			};


			ID3D11UnorderedAccessView* ppUAVTextures[] = {
				m_pUAVColor.Get()
			
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
				m_pSRVColor.Get()
			};


			ID3D11UnorderedAccessView* ppUAVTextures[] = {
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
	

	
		this->Blit(m_pSRVToneMap, pRTV);
		

		m_FrameIndex++;

	
		

	}

	auto RenderGUI(Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRTV) -> void override {

		
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
		m_FrameIndex = ImGui::SliderFloat("FieldOfView", &m_FieldOfView, 0.1f, 100.0f) ? 0 : m_FrameIndex;
		m_FrameIndex = ImGui::SliderInt("Step count", (int32_t*)&m_StepCount, 1, 512) ? 0 : m_FrameIndex;
		m_FrameIndex = ImGui::SliderInt("Trace depth", (int32_t*)&m_TraceDepth, 1, 10) ? 0 : m_FrameIndex;
		m_FrameIndex = ImGui::SliderFloat("Zoom", &m_Zoom, 0.1f, 10.0f) ? 0 : m_FrameIndex;
	
			

		ImGui::NewLine();

		
		
		ImGui::PlotLines("Opacity",
			[](void* data, int idx) { return static_cast<ScalarTransferFunction1D*>(data)->Evaluate(idx / static_cast<F32>(256)); },
			&m_OpacityTransferFunc, m_SamplingCount);

		ImGui::NewLine();
		ImGui::SliderFloat("Exposure", &m_Exposure, 4.0f, 100.0f);

		ImGui::End();
	

	

	}



private:
	
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pUAVVolumeIntensity;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pSRVVolumeIntensity;
	
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVDiffuseTF;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVSpecularTF;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVRoughnessTF;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVOpasityTF;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRVEnviroment;


	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pSRVColor;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pSRVColorSum;

	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pUAVColor;	
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pUAVColorSum;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pSRVToneMap;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pUAVToneMap;

	

	Microsoft::WRL::ComPtr<ID3D11VertexShader>    m_pVSBlit;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>     m_pPSBlit;

	Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_pCSPathTracingFirstPass;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_pCSPathTracingSecondPass;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_pCSToneMap;


	Microsoft::WRL::ComPtr<ID3D11SamplerState>  m_pSamplerPoint;
	Microsoft::WRL::ComPtr<ID3D11SamplerState>  m_pSamplerLinear;
	Microsoft::WRL::ComPtr<ID3D11SamplerState>  m_pSamplerAnisotropic;

	Microsoft::WRL::ComPtr<ID3D11Buffer>        m_pCBFrame;

	ColorTransferFunction1D                     m_DiffuseTransferFunc;
	ColorTransferFunction1D                     m_SpecularTransferFunc;
	ColorTransferFunction1D                     m_EmissionTransferFunc;
	ScalarTransferFunction1D                    m_RoughnessTransferFunc;
	ScalarTransferFunction1D                    m_OpacityTransferFunc;


	Hawk::Components::Camera  m_Camera;

	Hawk::Math::Vec3          m_BoundingBoxMin = Hawk::Math::Vec3(-0.5f, -0.5f, -0.5f);
	Hawk::Math::Vec3          m_BoundingBoxMax = Hawk::Math::Vec3(+0.5f, +0.5f, +0.5f);

	F32      m_DeltaTime        = 0.0f;
	F32      m_RotateSensivity  = 0.25f;
	F32      m_ZoomSensivity    = 1.5f;
	F32      m_Density          = 30.0f;
	F32      m_Exposure         = 80.0f;
	F32      m_FieldOfView      = 60.0f;
	F32      m_DenoiserStrange  = 1.0f;
	F32      m_Zoom             = 1.0f;
	uint32_t m_TraceDepth       = 2;
	uint32_t m_StepCount        = 255;
	uint32_t m_FrameIndex       = 0;
	uint32_t m_SamplingCount    = 256;

	bool     m_IsReloadShader      = false;
	bool     m_IsReloadTranferFunc = false;
	

	uint16_t m_DimensionX = 0;
	uint16_t m_DimensionY = 0;
	uint16_t m_DimensionZ = 0;

	std::random_device                  m_RandomDevice;
	std::mt19937                        m_RandomGenerator;
	std::uniform_real_distribution<F32> m_RandomDistribution;
};
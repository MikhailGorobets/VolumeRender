#pragma once

#include "Common.h"



struct ApplicationDesc {
	std::string Tittle = "Application <DX11>";
	uint32_t Width  = 800;
	uint32_t Height = 600;
	bool     IsFullScreen = false;
	bool     IsVSync = false;
	
};

class Application {
public:
	Application(ApplicationDesc const& desc);
	virtual ~Application();

	virtual auto EventMouseWheel(F32 delta)                                       -> void = 0;
	virtual auto EventMouseMove(F32 x, F32 y)                                     -> void = 0;
	virtual auto Update(F32 delta)                                                -> void = 0;
	virtual auto RenderFrame(Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRTV) -> void = 0;	
	virtual auto RenderGUI(Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRTV)   -> void = 0;
	virtual auto Run()                                                            -> void final;
	

private:
	auto InitializeSDL()      -> void;
	auto InitializeDirectX()  -> void;
	auto InitializeImGUI()    -> void;
	auto CalculateFrameTime() -> F32;
protected:

	SDL_Window*                                    m_pWindow;
	Microsoft::WRL::ComPtr<ID3D11Device>           m_pDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>    m_pImmediateContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain>         m_pSwapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_pRTV;
	std::chrono::high_resolution_clock::time_point m_LastFrame;
	ApplicationDesc                                m_ApplicationDesc;
};

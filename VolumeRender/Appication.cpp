#include "Application.h"

#include <SDL.h>
#include <SDL_syswm.h>
#include <ImGui/imgui_impl_dx11.h>
#include <ImGui/imgui_impl_sdl.h>

Application::Application(ApplicationDesc const& desc) {
    m_ApplicationDesc = desc;
    this->InitializeSDL();
    this->InitializeDirectX();
    this->InitializeImGUI();
}

Application::~Application() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyWindow(m_pWindow);
    SDL_Quit();
}

auto Application::Run() -> void {
    bool isRun = true;
    while (isRun) {
        SDL_PumpEvents();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_MOUSEWHEEL:
                this->EventMouseWheel(static_cast<float>(event.wheel.y)); break;
            case SDL_QUIT:
                isRun = false; break;
            default: break;
            }
        }

        if (auto point = std::make_tuple(0, 0); SDL_GetRelativeMouseState(&std::get<0>(point), &std::get<1>(point)) & SDL_BUTTON(SDL_BUTTON_RIGHT))
            this->EventMouseMove(static_cast<float>(std::get<0>(point)), static_cast<float>(std::get<1>(point)));

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplSDL2_NewFrame(m_pWindow);
        ImGui::NewFrame();

        this->Update(this->CalculateFrameTime());

        m_pImmediateContext->ClearRenderTargetView(m_pRTV.Get(), std::data({ 0.0f, 0.0f, 0.0f, 1.0f }));

        this->RenderFrame(m_pRTV);
        this->RenderGUI(m_pRTV);

        ImGui::Render();
        m_pImmediateContext->OMSetRenderTargets(1, m_pRTV.GetAddressOf(), nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        m_pSwapChain->Present(m_ApplicationDesc.IsVSync ? 1 : 0, 0);
    }
}

auto Application::InitializeSDL() -> void {
    SDL_Init(SDL_INIT_EVERYTHING);
    m_pWindow = SDL_CreateWindow(m_ApplicationDesc.Tittle.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, m_ApplicationDesc.Width, m_ApplicationDesc.Height, SDL_WINDOW_SHOWN);
}

auto Application::InitializeDirectX() -> void {
    SDL_SysWMinfo windowInfo = {};
    SDL_GetWindowWMInfo(m_pWindow, &windowInfo);

    {
        DXGI_SWAP_CHAIN_DESC desc = {};
        desc.BufferCount = 2;
        desc.BufferDesc.Width = m_ApplicationDesc.Width;
        desc.BufferDesc.Height = m_ApplicationDesc.Height;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        desc.BufferDesc.RefreshRate.Numerator = 60;
        desc.BufferDesc.RefreshRate.Denominator = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = windowInfo.info.win.window;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Windowed = !m_ApplicationDesc.IsFullScreen;
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL pFutureLever[] = { D3D_FEATURE_LEVEL_11_0 };
        uint32_t createFlag = 0;

#ifdef _DEBUG
        createFlag |= D3D11_CREATE_DEVICE_DEBUG;
#endif	
        DX::ThrowIfFailed(D3D11CreateDeviceAndSwapChain(nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createFlag,
            pFutureLever,
            _countof(pFutureLever),
            D3D11_SDK_VERSION,
            &desc,
            m_pSwapChain.GetAddressOf(),
            m_pDevice.GetAddressOf(), nullptr,
            nullptr));

        {
            Microsoft::WRL::ComPtr<ID3D11Texture2D> pBackBuffer;
            DX::ThrowIfFailed(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>(pBackBuffer.GetAddressOf())));
            DX::ThrowIfFailed(m_pDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, m_pRTV.GetAddressOf()));
        }
    }
    m_pDevice->GetImmediateContext(m_pImmediateContext.GetAddressOf());
}

auto Application::InitializeImGUI() -> void {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    auto& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("Data/Fonts/Roboto-Medium.ttf", 14.0f);

    ImGui::StyleColorsDark();
    ImGui_ImplDX11_Init(m_pDevice.Get(), m_pImmediateContext.Get());
    ImGui_ImplSDL2_InitForVulkan(m_pWindow);
}

auto Application::CalculateFrameTime() -> float {
    auto nowTime = std::chrono::high_resolution_clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::duration<float>>(nowTime - m_LastFrame).count();
    m_LastFrame = nowTime;
    return delta;
}
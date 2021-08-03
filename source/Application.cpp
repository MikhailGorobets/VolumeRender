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

#include "Application.h"

#include <glfw/glfw3.h>
#include <glfw/glfw3native.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui/imgui_impl_glfw.h>
#include <fmt/printf.h>

Application::Application(ApplicationDesc const& desc) {
    m_ApplicationDesc = desc;
    this->InitializeSDL();
    this->InitializeDirectX();
    this->InitializeImGUI();
}

Application::~Application() {   
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(m_pWindow);
    glfwTerminate();
}



auto Application::Run() -> void {

    
    glfwSetWindowUserPointer(m_pWindow, this);
    glfwSetMouseButtonCallback(m_pWindow, GLFWwindowCallbacks::MouseButtonCallback);
    glfwSetCursorPosCallback(m_pWindow, GLFWwindowCallbacks::MouseMoveCallback);
   

// glfwGetWindowSize(pWindow, &windowWidth, &windowHeight);
//
// double xpos, ypos;
// glfwGetCursorPos(pWindow, &xpos, &ypos);
// 
// 
//
//  pApplication->EventMouseMove(static_cast<float>(xpos), static_cast<float>(ypos));

    while (!glfwWindowShouldClose(m_pWindow)) {
        glfwPollEvents();
       // SDL_Event event;
       // while (SDL_PollEvent(&event)) {
       //     switch (event.type) {
       //     case SDL_MOUSEWHEEL:
       //         this->EventMouseWheel(static_cast<float>(event.wheel.y)); break;
       //     case SDL_QUIT:
       //         isRun = false; break;
       //     default: break;
       //     }
       // }

       // if (auto point = std::make_tuple(0, 0); SDL_GetRelativeMouseState(&std::get<0>(point), &std::get<1>(point)) & SDL_BUTTON(SDL_BUTTON_RIGHT))
       //     this->EventMouseMove(static_cast<float>(std::get<0>(point)), static_cast<float>(std::get<1>(point)));

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        this->Update(this->CalculateFrameTime());
    
        uint32_t frameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
        m_pD3D11On12Device->AcquireWrappedResources(m_pD3D11BackBuffersDummy[frameIndex].GetAddressOf(), 1);
        m_pD3D11On12Device->ReleaseWrappedResources(m_pD3D11BackBuffersDummy[frameIndex].GetAddressOf(), 1);

        m_pD3D11On12Device->AcquireWrappedResources(m_pD3D11BackBuffers[frameIndex].GetAddressOf(), 1);
        this->RenderFrame(m_pRTV[frameIndex]);
        this->RenderGUI(m_pRTV[frameIndex]);

        ImGui::Render();
        m_pAnnotation->BeginEvent(L"Render pass: ImGui");
        m_pImmediateContext->OMSetRenderTargets(1, m_pRTV[frameIndex].GetAddressOf(), nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        m_pImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);
        m_pAnnotation->EndEvent();
        m_pD3D11On12Device->ReleaseWrappedResources(m_pD3D11BackBuffers[frameIndex].GetAddressOf(), 1);
        m_pImmediateContext->Flush();
      
        m_pSwapChain->Present(m_ApplicationDesc.IsVSync ? 1 : 0, 0); 
    }   

    this->WaitForGPU();
}

auto Application::InitializeSDL() -> void {
    glfwInit();
    m_pWindow = glfwCreateWindow(m_ApplicationDesc.Width, m_ApplicationDesc.Height, m_ApplicationDesc.Tittle.c_str(), nullptr, nullptr);
}

auto Application::InitializeDirectX() -> void {
    HWND hWindow = glfwGetWin32Window(m_pWindow);
    uint32_t dxgiFactoryFlags = 0;
    uint32_t d3d11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
    {
        DX::ComPtr<ID3D12Debug> pDebugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugController)))) {
            pDebugController->EnableDebugLayer();
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            d3d11DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;     
        }
    }
#endif

    auto GetHardwareAdapter = [](DX::ComPtr<IDXGIFactory> pFactorty) -> DX::ComPtr<IDXGIAdapter1> {
        DX::ComPtr<IDXGIAdapter1> pAdapter;
        DX::ComPtr<IDXGIFactory6> pFactortyNew;
        pFactorty.As(&pFactortyNew);
      
        for (uint32_t adapterID = 0; DXGI_ERROR_NOT_FOUND != pFactortyNew->EnumAdapterByGpuPreference(adapterID, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&pAdapter)); adapterID++) {
            DXGI_ADAPTER_DESC1 desc = {};
            pAdapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) 
                continue;
           
            if (SUCCEEDED(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr))) 
                break;            
        }
        return pAdapter;
    };


    DX::ComPtr<IDXGIFactory4> pFactory;
    DX::ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&pFactory)));

    DX::ComPtr<IDXGIAdapter1> pAdapter = GetHardwareAdapter(pFactory);
    DX::ThrowIfFailed(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_pD3D12Device)));
    
#if defined(_DEBUG)
    DX::ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(m_pD3D12Device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {

        D3D12_MESSAGE_SEVERITY severities[] = {
            D3D12_MESSAGE_SEVERITY_INFO,
        };

        D3D12_MESSAGE_ID denyIds[] = {
            D3D12_MESSAGE_ID_INVALID_DESCRIPTOR_HANDLE
        };

        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        DX::ThrowIfFailed(infoQueue->PushStorageFilter(&filter));
    }
#endif    

    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        DX::ThrowIfFailed(m_pD3D12Device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_pD3D12CmdQueue)));
    }
   
    {
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.BufferCount = FrameCount;
        desc.Width = m_ApplicationDesc.Width;
        desc.Height = m_ApplicationDesc.Height;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.SampleDesc.Count = 1;

        DX::ComPtr<IDXGISwapChain1> pSwapChain;
        DX::ThrowIfFailed(pFactory->CreateSwapChainForHwnd(m_pD3D12CmdQueue.Get(), hWindow, &desc, nullptr, nullptr,  &pSwapChain));
        DX::ThrowIfFailed(pFactory->MakeWindowAssociation(hWindow, DXGI_MWA_NO_ALT_ENTER));
        DX::ThrowIfFailed(pSwapChain.As(&m_pSwapChain));

    }
   
    {     
        DX::ThrowIfFailed(D3D11On12CreateDevice(m_pD3D12Device.Get(), d3d11DeviceFlags, nullptr, 0, reinterpret_cast<IUnknown**>(m_pD3D12CmdQueue.GetAddressOf()), 1, 0, &m_pDevice, &m_pImmediateContext, nullptr));
        DX::ThrowIfFailed(m_pDevice.As(&m_pD3D11On12Device));
        DX::ThrowIfFailed(m_pImmediateContext.As(&m_pAnnotation));

        for (uint32_t frameID = 0; frameID < FrameCount; frameID++) {
            DX::ThrowIfFailed(m_pSwapChain->GetBuffer(frameID, IID_PPV_ARGS(&m_pD3D12BackBuffers[frameID])));

            D3D11_RESOURCE_FLAGS d3d11Flags = {D3D11_BIND_RENDER_TARGET};
            DX::ThrowIfFailed(m_pD3D11On12Device->CreateWrappedResource(m_pD3D12BackBuffers[frameID].Get(), &d3d11Flags, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET, IID_PPV_ARGS(&m_pD3D11BackBuffersDummy[frameID])));
            DX::ThrowIfFailed(m_pD3D11On12Device->CreateWrappedResource(m_pD3D12BackBuffers[frameID].Get(), &d3d11Flags, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT, IID_PPV_ARGS(&m_pD3D11BackBuffers[frameID])));
         
            D3D11_RENDER_TARGET_VIEW_DESC descRTV = {};
            descRTV.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            descRTV.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            DX::ThrowIfFailed(m_pDevice->CreateRenderTargetView(m_pD3D11BackBuffers[frameID].Get(), &descRTV, m_pRTV[frameID].GetAddressOf()));
        }
    }
    
    DX::ThrowIfFailed(m_pD3D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_pD3D12Fence.GetAddressOf())));
    m_FenceEvent = CreateEvent(nullptr, false, false, nullptr);
    if (m_FenceEvent == nullptr)
        DX::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    
    
}

auto Application::InitializeImGUI() -> void {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    auto& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("content/Fonts/Roboto-Medium.ttf", 14.0f);

    ImGui::StyleColorsDark();
    ImGui_ImplDX11_Init(m_pDevice.Get(), m_pImmediateContext.Get());
    ImGui_ImplGlfw_InitForOther(m_pWindow, false);
}

auto Application::CalculateFrameTime() -> float {
    auto nowTime = std::chrono::high_resolution_clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::duration<float>>(nowTime - m_LastFrame).count();
    m_LastFrame = nowTime;
    return delta;
}

auto Application::WaitForGPU() -> void {
    m_FenceValue++;

    DX::ThrowIfFailed(m_pD3D12CmdQueue->Signal(m_pD3D12Fence.Get(), m_FenceValue));
    DX::ThrowIfFailed(m_pD3D12Fence->SetEventOnCompletion(m_FenceValue, m_FenceEvent));
    WaitForSingleObjectEx(m_FenceEvent, INFINITE, false);
}

auto GLFWwindowCallbacks::MouseButtonCallback(GLFWwindow* pWindow, int32_t button, int32_t action, int32_t modes) -> void {
    auto pApplication = reinterpret_cast<Application*>(glfwGetWindowUserPointer(pWindow));
    auto pState = &pApplication->m_GLFWState;
    
    auto MouseButtonCall = [&](GLFWWindowState::MouseButton button, int32_t action) -> void {
        switch (action) {
            case GLFW_PRESS:
                pState->MouseState[button] = GLFWWindowState::MousePress;
                break;
            case GLFW_RELEASE:
                pState->MouseState[button] = GLFWWindowState::MouseRelease;
                break;
            default:
                break;
        }
    };

    switch (button) {
        case GLFW_MOUSE_BUTTON_RIGHT:
            MouseButtonCall(GLFWWindowState::MouseButtonRight, action);
            break;
        case GLFW_MOUSE_BUTTON_LEFT:
            MouseButtonCall(GLFWWindowState::MouseButtonLeft, action);
            break;
        default:
            break;
    }  

}

auto GLFWwindowCallbacks::MouseMoveCallback(GLFWwindow* pWindow, double mousePosX, double mousePosY) -> void {
    auto pApplication = reinterpret_cast<Application*>(glfwGetWindowUserPointer(pWindow));
    auto pState = &pApplication->m_GLFWState;

    if (pState->MouseState[GLFWWindowState::MouseButtonRight] == GLFWWindowState::MousePress) {
        if (pState->IsFirstPress) {
            pState->PreviousMousePositionX = mousePosX;
            pState->PreviousMousePositionY = mousePosY;
            pState->IsFirstPress = false;
        } else {
            double dX = mousePosX - pState->PreviousMousePositionX;
            double dY = mousePosY - pState->PreviousMousePositionY;

            pState->PreviousMousePositionX = mousePosX;
            pState->PreviousMousePositionY = mousePosY;
           
            pApplication->EventMouseMove(static_cast<float>(dX), static_cast<float>(dY));
            
        }        
    } else {
        pState->IsFirstPress = true;
    }

}
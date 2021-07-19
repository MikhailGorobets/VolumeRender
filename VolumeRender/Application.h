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

struct ApplicationDesc {
    std::string Tittle = "Application <DX11>";
    uint32_t Width = 800;
    uint32_t Height = 600;
    bool     IsFullScreen = false;
    bool     IsVSync = false;
};

struct SDL_Window;

class Application {
public:
    Application(ApplicationDesc const& desc);

    virtual ~Application();

    virtual auto EventMouseWheel(float delta) -> void = 0;

    virtual auto EventMouseMove(float x, float y) -> void = 0;

    virtual auto Update(float delta) -> void = 0;

    virtual auto RenderFrame(Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRTV) -> void = 0;

    virtual auto RenderGUI(Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRTV) -> void = 0;

    virtual auto Run() -> void final;

private:
    auto InitializeSDL() -> void;

    auto InitializeDirectX() -> void;

    auto InitializeImGUI() -> void;

    auto CalculateFrameTime() -> float;

protected:
    SDL_Window* m_pWindow;
    Microsoft::WRL::ComPtr<ID3D11Device>           m_pDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>    m_pImmediateContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain>         m_pSwapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_pRTV;
    std::chrono::high_resolution_clock::time_point m_LastFrame;
    ApplicationDesc                                m_ApplicationDesc;
};
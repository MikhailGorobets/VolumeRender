/*
 * MIT License
 *
 * Copyright(c) 2021-2023 Mikhail Gorobets
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

struct GLFWwindow;

struct GLFW_WindowCallbacks {
	static void MouseButtonCallback(GLFWwindow* pWindow, int32_t button, int32_t action, int32_t modes);
	static void MouseMoveCallback(GLFWwindow* pWindow, double mousePosX, double mousePosY);
	static void MouseScrollCallback(GLFWwindow* pWindow, double offsetX, double offsetY);
	static void ResizeWindowCallback(GLFWwindow* pWindow, int32_t width, int32_t height);
};

struct GLFWWindowState {
	enum MouseButton : uint8_t {
		MouseButtonLeft,
		MouseButtonRight,
		MouseButtonMiddle,
		MouseButtonMax
	};

	enum MouseState : uint8_t {
		MouseRelease,
		MousePress
	};

	MouseState MouseState[MouseButtonMax] = {};
	double PreviousMousePositionX = {};
	double PreviousMousePositionY = {};
};


class Application{
	friend GLFW_WindowCallbacks;
public:
	Application(ApplicationDesc const& desc);

	virtual ~Application();

	virtual void EventMouseWheel(float delta) = 0;

	virtual void EventMouseMove(float x, float y) = 0;

	virtual void Resize(int32_t width, int32_t height);

	virtual void Update(float delta) = 0;

	virtual void RenderFrame(DX::ComPtr<ID3D11RenderTargetView> pRTV) = 0;

	virtual void RenderGUI(DX::ComPtr<ID3D11RenderTargetView> pRTV) = 0;

	auto Run() -> void;

private:
	void InitializeSDL();

	void InitializeD3D11();

	void InitializeImGUI();

	float CalculateFrameTime();

	void WaitForGPU();

protected:
	using TimePoint = std::chrono::high_resolution_clock::time_point;

	static constexpr uint32_t FrameCount = 3;

	GLFWwindow* m_pWindow = {};
	HANDLE      m_FenceEvent = {};
	uint32_t    m_FenceValue = {};
	DX::ComPtr<ID3D12Device>       m_pD3D12Device;
	DX::ComPtr<ID3D11On12Device>   m_pD3D11On12Device;
	DX::ComPtr<ID3D12CommandQueue> m_pD3D12CmdQueue;
	DX::ComPtr<ID3D12Fence>        m_pD3D12Fence;
	DX::ComPtr<ID3D12Resource>     m_pD3D12BackBuffers[FrameCount];
	DX::ComPtr<ID3D11Resource>     m_pD3D11BackBuffersDummy[FrameCount];
	DX::ComPtr<ID3D11Resource>     m_pD3D11BackBuffers[FrameCount];

	DX::ComPtr<ID3D11Device>              m_pDevice;
	DX::ComPtr<ID3D11DeviceContext>       m_pImmediateContext;
	DX::ComPtr<IDXGISwapChain3>           m_pSwapChain;
	DX::ComPtr<ID3DUserDefinedAnnotation> m_pAnnotation;
	DX::ComPtr<ID3D11RenderTargetView>    m_pRTV[FrameCount];
	TimePoint       m_LastFrame;
	ApplicationDesc m_ApplicationDesc;
	GLFWWindowState m_GLFWState = {};
};

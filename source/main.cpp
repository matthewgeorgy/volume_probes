#define MG_USE_WINDOWS_H
#include <mg.h>
#include <dxgi.h>
#include <d3d11.h>
#include <stdio.h>

#define SCR_WIDTH 		1024
#define SCR_HEIGHT		768

LRESULT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

int
main()
{
	//////////////////////////////////////////////////////////////////////////
	// Win32 setup

	ATOM Atom;
	WNDCLASSEX WndClass = {};
	HWND Window;
	MSG Message;
	RECT WindowRect;
	INT WindowWidth, WindowHeight;

	WndClass.cbSize = sizeof(WndClass);
	WndClass.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
	WndClass.lpfnWndProc = &WndProc;
	WndClass.lpszClassName = "VolumeProbesWindowClass";
	WndClass.hInstance = GetModuleHandleA(NULL);

	Atom = RegisterClassEx(&WndClass);
	if (!Atom)
	{
		printf("Failed to create register window class...!r\n");
		return (-1);
	}

	WindowRect.left = 0;
	WindowRect.right = SCR_WIDTH;
	WindowRect.top = 0;
	WindowRect.bottom = SCR_HEIGHT;

	AdjustWindowRect(&WindowRect, WS_OVERLAPPEDWINDOW, FALSE);

	WindowWidth = WindowRect.right - WindowRect.left;
	WindowHeight = WindowRect.bottom - WindowRect.top;

	Window = CreateWindowEx(0, WndClass.lpszClassName, "Volume Probes",
		WS_OVERLAPPEDWINDOW, 0, 0, WindowWidth, WindowHeight,
		NULL, NULL, WndClass.hInstance, NULL);
	if (!Window)
	{
		printf("Failed to create window...!\r\n");
		return (-1);
	}

	ShowWindow(Window, SW_SHOW);
	UpdateWindow(Window);

	//////////////////////////////////////////////////////////////////////////
	// D3D11 Setup

	HRESULT Hr;
	ID3D11Device *Device;
	ID3D11DeviceContext *Context;
	ID3D11Texture2D *Backbuffer,
					*DepthBuffer;
	ID3D11RenderTargetView *BackbufferRTV;
	ID3D11DepthStencilView *BackbufferDSV;
	ID3D11RasterizerState *RasterState;
	IDXGISwapChain *SwapChain;
	DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
	D3D11_TEXTURE2D_DESC DepthBufferDesc = {};
	D3D11_VIEWPORT Viewport;
	D3D11_RASTERIZER_DESC RasterStateDesc = {};

	SwapChainDesc.BufferDesc.Width = SCR_WIDTH;
	SwapChainDesc.BufferDesc.Height = SCR_HEIGHT;
	SwapChainDesc.BufferDesc.RefreshRate = {60, 1};
	SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SwapChainDesc.SampleDesc = {1, 0};
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.BufferCount = 2;
	SwapChainDesc.OutputWindow = Window;
	SwapChainDesc.Windowed = TRUE;
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	DepthBufferDesc.Width = SCR_WIDTH;
	DepthBufferDesc.Height = SCR_HEIGHT;
	DepthBufferDesc.MipLevels = 1;
	DepthBufferDesc.ArraySize = 1;
	DepthBufferDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DepthBufferDesc.SampleDesc = {1, 0};
	DepthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	DepthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	Hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
		D3D11_CREATE_DEVICE_DEBUG, 0, 0, D3D11_SDK_VERSION, &SwapChainDesc,
		&SwapChain, &Device, NULL, &Context);
	Hr = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&Backbuffer);
	Hr = Device->CreateRenderTargetView(Backbuffer, NULL, &BackbufferRTV);
	Hr = Device->CreateTexture2D(&DepthBufferDesc, NULL, &DepthBuffer);
	Hr = Device->CreateDepthStencilView(DepthBuffer, NULL, &BackbufferDSV);

	Viewport.Width = SCR_WIDTH;
	Viewport.Height = SCR_HEIGHT;
	Viewport.MaxDepth = 1.0f;

	RasterStateDesc.FillMode = D3D11_FILL_SOLID;
	RasterStateDesc.CullMode = D3D11_CULL_NONE;
	RasterStateDesc.DepthClipEnable = TRUE;
	Hr = Device->CreateRasterizerState(&RasterStateDesc, &RasterState);

	//////////////////////////////////////////////////////////////////////////
	// Main loop

	for (;;)
	{
		if (PeekMessage(&Message, NULL, 0, 0, PM_REMOVE))
		{
			if (Message.message == WM_QUIT)
			{
				break;
			}
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}
		else
		{
		}
	}

	return (0);
}

LRESULT CALLBACK
WndProc(HWND hWnd,
		UINT Msg,
		WPARAM wParam,
		LPARAM lParam)
{
	LRESULT Result = 0;

	switch (Msg)
	{
		case WM_KEYDOWN:
		{
			if (wParam == VK_ESCAPE)
			{
				PostQuitMessage(0);
			}
		} break;

		case WM_CLOSE:
		case WM_DESTROY:
		{
			PostQuitMessage(0);
		} break;

		default:
		{
			Result = DefWindowProc(hWnd, Msg, wParam, lParam);
		} break;
	}

	return (Result);
}


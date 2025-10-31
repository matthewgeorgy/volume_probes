#define MG_USE_WINDOWS_H
#define MG_IMPL
#include <mg.h>
#include <dxgi.h>
#include <d3d11.h>
#include <d3dcommon.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define SCR_WIDTH 		1024
#define SCR_HEIGHT		768

void		ProcessInput(GLFWwindow *Window);

int
main()
{
	//////////////////////////////////////////////////////////////////////////
	// GLFW setup

	GLFWwindow		*Window;
	HWND			hWnd;


	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	Window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Volume Probes", nullptr, nullptr);
	if (!Window)
	{
		printf("Failed to create window...!\r\n");
		return (-1);
	}

	hWnd = glfwGetWin32Window(Window);

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
	SwapChainDesc.OutputWindow = hWnd;
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
	// Shader setup

	ID3D11VertexShader		*TriangleVS;
	ID3D11PixelShader 		*TrianglePS;
	ID3DBlob 				*TriangleVSBlob,
			 				*TrianglePSBlob;


	Hr = D3DReadFileToBlob(L"build/triangle_vs.cso", &TriangleVSBlob);
	Hr = D3DReadFileToBlob(L"build/triangle_ps.cso", &TrianglePSBlob);
	Device->CreateVertexShader(TriangleVSBlob->GetBufferPointer(), TriangleVSBlob->GetBufferSize(), NULL, &TriangleVS);
	Device->CreatePixelShader(TrianglePSBlob->GetBufferPointer(), TrianglePSBlob->GetBufferSize(), NULL, &TrianglePS);

	//////////////////////////////////////////////////////////////////////////
	// Buffer setup

	ID3D11Buffer						*VertexBuffer,
										*IndexBuffer;
	ID3D11ShaderResourceView			*VertexBufferView;
	D3D11_BUFFER_DESC					VertexBufferDesc = {},
										IndexBufferDesc = {};
	D3D11_SUBRESOURCE_DATA				VertexBufferSubData = {},
										IndexBufferSubData = {};
	D3D11_SHADER_RESOURCE_VIEW_DESC		VertexBufferViewDesc;
	v3									Vertices[] =
	{
		v3(-0.5f, -0.5f, 0.5f),		v3(1.0f, 0.0f, 0.0f),
		v3( 0.5f, -0.5f, 0.5f),		v3(0.0f, 1.0f, 0.0f),
		v3( 0.5f,  0.5f, 0.5f),		v3(0.0f, 0.0f, 1.0f),
		v3(-0.5f,  0.5f, 0.5f),		v3(1.0f, 1.0f, 1.0f),
	};
	u32 								Indices[] =
	{
		0, 1, 2,
		0, 2, 3,
	};


	VertexBufferDesc.ByteWidth = sizeof(Vertices);
	VertexBufferDesc.StructureByteStride = 2 * sizeof(v3);
	VertexBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	VertexBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	VertexBufferSubData.pSysMem = Vertices;

	IndexBufferDesc.ByteWidth = sizeof(Indices);
	IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	IndexBufferSubData.pSysMem = Indices;

	Hr = Device->CreateBuffer(&VertexBufferDesc, &VertexBufferSubData, &VertexBuffer);
	Hr = Device->CreateBuffer(&IndexBufferDesc, &IndexBufferSubData, &IndexBuffer);

	VertexBufferViewDesc.Format = DXGI_FORMAT_UNKNOWN;
	VertexBufferViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	VertexBufferViewDesc.Buffer.FirstElement = 0;
	VertexBufferViewDesc.Buffer.NumElements = 4;

	Hr = Device->CreateShaderResourceView(VertexBuffer, &VertexBufferViewDesc, &VertexBufferView);
	
	//////////////////////////////////////////////////////////////////////////
	// Main loop

	while (!glfwWindowShouldClose(Window))
	{
		glfwPollEvents();
		ProcessInput(Window);

		static FLOAT ClearColor[4] = { 48.f / 255.f, 10.f / 255.f, 36.f / 255.f, 1.f };

		Context->OMSetRenderTargets(1, &BackbufferRTV, BackbufferDSV);
		Context->ClearRenderTargetView(BackbufferRTV, ClearColor);
		Context->ClearDepthStencilView(BackbufferDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		Context->RSSetViewports(1, &Viewport);
		Context->RSSetState(RasterState);

		Context->VSSetShader(TriangleVS, 0, 0);
		Context->PSSetShader(TrianglePS, 0, 0);

		Context->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
		Context->VSSetShaderResources(0, 1, &VertexBufferView);

		Context->DrawIndexed(6, 0, 0);

		SwapChain->Present(0, 0);
	}

	return (0);
}

void		
ProcessInput(GLFWwindow *Window)
{
	if (glfwGetKey(Window, GLFW_KEY_ESCAPE))
	{
		glfwSetWindowShouldClose(Window, GLFW_TRUE);
	}
}


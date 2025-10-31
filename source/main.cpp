#include <dxgi.h>
#include <d3d11.h>
#include <d3dcommon.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <vector>

#define MG_USE_WINDOWS_H
#define MG_IMPL
#include <mg.h>
#include <data.h>
#include <perlin.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui/imgui_impl_glfw.h>
#include <implot/implot.h>

#define SCR_WIDTH 		1920
#define SCR_HEIGHT		1080

#define VOLUME_WIDTH	64
#define VOLUME_HEIGHT	64
#define VOLUME_DEPTH	64
#define VOLUME_SIZE		VOLUME_WIDTH * VOLUME_HEIGHT * VOLUME_DEPTH

struct model_params
{
	m4		World,
			View,
			Proj;
};

struct camera
{
	v3		Pos,
			Front,
			Up;
};

struct raymarch_params
{
	u32		ScreenWidth,
			ScreenHeight;
	f32		MinVal,
			MaxVal;
	v3		LightPos;
	f32		_Pad0;
};

camera gCamera;
f32 gDeltaTime = 0;
f32 gLastFrame = 0;
b32 gFirstMouse = TRUE;
b32 gImGuiControl = FALSE;

ID3D11ShaderResourceView		*NullSRV[8] = {};

void		ProcessInput(GLFWwindow *Window);
void		MouseCallback(GLFWwindow *Window, f64 XPos, f64 YPos);

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

 	glfwSetCursorPosCallback(Window, &MouseCallback);
    glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	hWnd = glfwGetWin32Window(Window);

	//////////////////////////////////////////////////////////////////////////
	// D3D11 Setup

	HRESULT 					Hr;
	ID3D11Device 				*Device;
	ID3D11DeviceContext 		*Context;
	ID3D11Texture2D 			*Backbuffer,
								*DepthBuffer;
	ID3D11RenderTargetView		*BackbufferRTV;
	ID3D11DepthStencilView 		*BackbufferDSV;
	ID3D11RasterizerState 		*CullFront,
						  		*CullBack,
						  		*CullNone;
	IDXGISwapChain 				*SwapChain;
	DXGI_SWAP_CHAIN_DESC 		SwapChainDesc = {};
	D3D11_TEXTURE2D_DESC 		DepthBufferDesc = {};
	D3D11_VIEWPORT 				Viewport = {};
	D3D11_RASTERIZER_DESC 		RasterStateDesc = {};

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
	Hr = Device->CreateRasterizerState(&RasterStateDesc, &CullNone);

	RasterStateDesc.CullMode = D3D11_CULL_FRONT;
	Hr = Device->CreateRasterizerState(&RasterStateDesc, &CullFront);

	RasterStateDesc.CullMode = D3D11_CULL_BACK;
	Hr = Device->CreateRasterizerState(&RasterStateDesc, &CullBack);

	//////////////////////////////////////////////////////////////////////////
	// Shader setup

	ID3D11VertexShader		*ModelVS,
							*RaymarchVS,
							*LampVS;
	ID3D11PixelShader 		*ModelPS,
							*RaymarchPS,
							*LampPS;
	ID3DBlob 				*ModelVSBlob,
			 				*ModelPSBlob,
							*RaymarchVSBlob,
			 				*RaymarchPSBlob,
							*LampVSBlob,
			 				*LampPSBlob;


	Hr = D3DReadFileToBlob(L"build/model_vs.cso", &ModelVSBlob);
	Hr = D3DReadFileToBlob(L"build/model_ps.cso", &ModelPSBlob);
	Hr = D3DReadFileToBlob(L"build/raymarch_vs.cso", &RaymarchVSBlob);
	Hr = D3DReadFileToBlob(L"build/raymarch_ps.cso", &RaymarchPSBlob);
	Hr = D3DReadFileToBlob(L"build/lamp_vs.cso", &LampVSBlob);
	Hr = D3DReadFileToBlob(L"build/lamp_ps.cso", &LampPSBlob);
	Device->CreateVertexShader(ModelVSBlob->GetBufferPointer(), ModelVSBlob->GetBufferSize(), NULL, &ModelVS);
	Device->CreatePixelShader(ModelPSBlob->GetBufferPointer(), ModelPSBlob->GetBufferSize(), NULL, &ModelPS);
	Device->CreateVertexShader(RaymarchVSBlob->GetBufferPointer(), RaymarchVSBlob->GetBufferSize(), NULL, &RaymarchVS);
	Device->CreatePixelShader(RaymarchPSBlob->GetBufferPointer(), RaymarchPSBlob->GetBufferSize(), NULL, &RaymarchPS);
	Device->CreateVertexShader(LampVSBlob->GetBufferPointer(), LampVSBlob->GetBufferSize(), NULL, &LampVS);
	Device->CreatePixelShader(LampPSBlob->GetBufferPointer(), LampPSBlob->GetBufferSize(), NULL, &LampPS);

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


	VertexBufferDesc.ByteWidth = sizeof(Vertices);
	VertexBufferDesc.StructureByteStride = sizeof(v3);
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
	VertexBufferViewDesc.Buffer.NumElements = 8;

	Hr = Device->CreateShaderResourceView(VertexBuffer, &VertexBufferViewDesc, &VertexBufferView);

	//////////////////////////////////////////////////////////////////////////
	// Render targets

	ID3D11Texture2D						*FrontTexture,
										*BackTexture;
	ID3D11ShaderResourceView			*FrontSRV,
										*BackSRV;
	ID3D11RenderTargetView				*FrontRTV,
										*BackRTV;
	D3D11_TEXTURE2D_DESC				RenderTextureDesc = {};
	D3D11_SHADER_RESOURCE_VIEW_DESC		RenderTextureSRVDesc = {};
	D3D11_RENDER_TARGET_VIEW_DESC		RenderTextureRTVDesc = {};


	RenderTextureDesc.Width = SCR_WIDTH;
	RenderTextureDesc.Height = SCR_HEIGHT;
	RenderTextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	RenderTextureDesc.SampleDesc = {1, 0};
	RenderTextureDesc.MipLevels = 1;
	RenderTextureDesc.ArraySize = 1;
	RenderTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

	RenderTextureSRVDesc.Format = RenderTextureDesc.Format;
	RenderTextureSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	RenderTextureSRVDesc.Texture2D.MipLevels = 1;
	RenderTextureSRVDesc.Texture2D.MostDetailedMip = 0;

	RenderTextureRTVDesc.Format = RenderTextureDesc.Format;
	RenderTextureRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	RenderTextureRTVDesc.Texture2D.MipSlice = 0;

	Device->CreateTexture2D(&RenderTextureDesc, nullptr, &FrontTexture);
	Device->CreateTexture2D(&RenderTextureDesc, nullptr, &BackTexture);
	Device->CreateShaderResourceView(FrontTexture, &RenderTextureSRVDesc, &FrontSRV);
	Device->CreateShaderResourceView(BackTexture, &RenderTextureSRVDesc, &BackSRV);
	Device->CreateRenderTargetView(FrontTexture, &RenderTextureRTVDesc, &FrontRTV);
	Device->CreateRenderTargetView(BackTexture, &RenderTextureRTVDesc, &BackRTV);

	//////////////////////////////////////////////////////////////////////////
	// Sampler

	ID3D11SamplerState		*LinearSampler;
	D3D11_SAMPLER_DESC		LinearSamplerDesc = {};


	LinearSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	LinearSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	LinearSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	LinearSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	LinearSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	LinearSamplerDesc.MinLOD = 0.f;
	LinearSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	Device->CreateSamplerState(&LinearSamplerDesc, &LinearSampler);

	//////////////////////////////////////////////////////////////////////////
	// Volume texture

	ID3D11Texture3D						*Volume;
	ID3D11ShaderResourceView			*VolumeSRV;
	D3D11_TEXTURE3D_DESC				VolumeDesc = {};
	D3D11_SHADER_RESOURCE_VIEW_DESC		VolumeSRVDesc = {};
	D3D11_SUBRESOURCE_DATA				VolumeSubData = {};
	f32									*VolumeData;
	f32									MinNoise =  10000.0f,
										MaxNoise = -10000.0f;
	DWORD 								Read;
	HANDLE 								File;


	VolumeData = (f32 *)HeapAlloc(GetProcessHeap(), 0, VOLUME_SIZE * sizeof(*VolumeData));

	File = CreateFile("assets/cloud64.bin", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	ReadFile(File, VolumeData, VOLUME_SIZE * sizeof(*VolumeData), &Read, NULL);
	printf("Read %u bytes out of %llu\n", Read, VOLUME_SIZE * sizeof(*VolumeData));
	CloseHandle(File);

	for (u32 i = 0; i < VOLUME_SIZE; i++)
	{
		f32 Density = VolumeData[i];

		if (Density > MaxNoise)
		{
			MaxNoise = Density;
		}
		if (Density < MinNoise)
		{
			MinNoise = Density;
		}
	}
	
	VolumeDesc.Width = VOLUME_WIDTH;
	VolumeDesc.Height = VOLUME_HEIGHT;
	VolumeDesc.Depth = VOLUME_DEPTH;
	VolumeDesc.Format = DXGI_FORMAT_R32_FLOAT;
	VolumeDesc.MipLevels = 1;
	VolumeDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	VolumeSubData.pSysMem = VolumeData;
	VolumeSubData.SysMemPitch = VolumeDesc.Width * sizeof(f32);
	VolumeSubData.SysMemSlicePitch = VolumeDesc.Width * VolumeDesc.Height * sizeof(f32);

	VolumeSRVDesc.Format = VolumeDesc.Format;
	VolumeSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	VolumeSRVDesc.Texture3D.MipLevels = 1;
	VolumeSRVDesc.Texture3D.MostDetailedMip = 0;

	Device->CreateTexture3D(&VolumeDesc, &VolumeSubData, &Volume);
	Device->CreateShaderResourceView(Volume, &VolumeSRVDesc, &VolumeSRV);

	HeapFree(GetProcessHeap(), 0, VolumeData);

	//////////////////////////////////////////////////////////////////////////
	// Params

	model_params			ModelParams = {};
	raymarch_params			RaymarchParams = {};
	ID3D11Buffer			*ModelParamsBuffer,
							*RaymarchParamsBuffer;
	D3D11_BUFFER_DESC		ModelParamsBufferDesc = {},
							RaymarchParamsBufferDesc = {};


	ModelParamsBufferDesc.ByteWidth = sizeof(ModelParams);
	ModelParamsBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	RaymarchParamsBufferDesc.ByteWidth = sizeof(RaymarchParams);
	RaymarchParamsBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	Device->CreateBuffer(&ModelParamsBufferDesc, nullptr, &ModelParamsBuffer);
	Device->CreateBuffer(&RaymarchParamsBufferDesc, nullptr, &RaymarchParamsBuffer);

	gCamera.Pos = v3(3, 1.5f, -3.5f);
	gCamera.Front = v3(-0.5f, -0.25f, 0.8f);
	gCamera.Up = v3(0, 1, 0);	

	RaymarchParams.ScreenWidth = SCR_WIDTH;
	RaymarchParams.ScreenHeight = SCR_HEIGHT;
	RaymarchParams.MinVal = MinNoise;
	RaymarchParams.MaxVal = MaxNoise;
	RaymarchParams.LightPos = v3(1, 1, 1);

	//////////////////////////////////////////////////////////////////////////
	// ImGui setup

	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGuiIO &IO = ImGui::GetIO();
	(void)IO;

	ImGui_ImplGlfw_InitForOther(Window, true);
	ImGui_ImplDX11_Init(Device, Context);

	//////////////////////////////////////////////////////////////////////////
	// Colormap

	//////////////////////////////////////////////////////////////////////////
	// Colormap

	ID3D11Texture1D						*Colormap;
	ID3D11ShaderResourceView			*ColormapSRV;
	D3D11_TEXTURE1D_DESC				ColormapDesc = {};
	D3D11_SHADER_RESOURCE_VIEW_DESC		ColormapSRVDesc = {};
	D3D11_SUBRESOURCE_DATA				ColormapSubData = {};
	u32									ColormapSize = 512;
	v4									*ColormapData;


	ColormapData = (v4 *)HeapAlloc(GetProcessHeap(), 0, ColormapSize * sizeof(*ColormapData));

	f32 t = 0;
	f32 dt = 1.0f / (ColormapSize - 1);

	for (u32 I = 0; I < ColormapSize; I++)
	{
		ColormapData[I].x = ImPlot::SampleColormap(t, ImPlotColormap_Spectral).x;
		ColormapData[I].y = ImPlot::SampleColormap(t, ImPlotColormap_Spectral).y;
		ColormapData[I].z = ImPlot::SampleColormap(t, ImPlotColormap_Spectral).z;
		ColormapData[I].w = ImPlot::SampleColormap(t, ImPlotColormap_Spectral).w;

		t += dt;
	}

	ColormapDesc.Width = ColormapSize;
	ColormapDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	ColormapDesc.MipLevels = 1;
	ColormapDesc.ArraySize = 1;
	ColormapDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	ColormapSubData.pSysMem = ColormapData;
	ColormapSubData.SysMemPitch = ColormapDesc.Width * sizeof(*ColormapData);

	ColormapSRVDesc.Format = ColormapDesc.Format;
	ColormapSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
	ColormapSRVDesc.Texture1D.MipLevels = 1;
	ColormapSRVDesc.Texture1D.MostDetailedMip = 0;

	Device->CreateTexture1D(&ColormapDesc, &ColormapSubData, &Colormap);
	Device->CreateShaderResourceView(Colormap, &ColormapSRVDesc, &ColormapSRV);	

	HeapFree(GetProcessHeap(), 0, ColormapData);

	//////////////////////////////////////////////////////////////////////////
	// Main loop

	while (!glfwWindowShouldClose(Window))
	{
		glfwPollEvents();
		ProcessInput(Window);

		f32 CurrentFrame = f32(glfwGetTime());
		gDeltaTime = CurrentFrame - gLastFrame;
		gLastFrame = CurrentFrame;

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Foo");
			ImGui::DragFloat("Light X", &RaymarchParams.LightPos.x, 0.01f, -5, 5);
			ImGui::DragFloat("Light Y", &RaymarchParams.LightPos.y, 0.01f, -5, 5);
			ImGui::DragFloat("Light Z", &RaymarchParams.LightPos.z, 0.01f, -5, 5);
		ImGui::End();
		Context->UpdateSubresource(RaymarchParamsBuffer, 0, 0, &RaymarchParams, 0, 0);

		static f32 ClearColor[4] = { 0, 0, 0, 1 };

		Context->ClearDepthStencilView(BackbufferDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		Context->ClearRenderTargetView(BackbufferRTV, ClearColor);
		Context->ClearRenderTargetView(FrontRTV, ClearColor);
		Context->ClearRenderTargetView(BackRTV, ClearColor);
		Context->RSSetViewports(1, &Viewport);
		Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		ModelParams.World = Mat4Identity();
		ModelParams.View = Mat4LookAtLH(gCamera.Pos, gCamera.Pos + gCamera.Front, gCamera.Up);
		ModelParams.Proj = Mat4PerspectiveLH(45.0f, (f32)SCR_WIDTH / (f32)SCR_HEIGHT, 0.1f, 1000.0f);
		Context->UpdateSubresource(ModelParamsBuffer, 0, 0, &ModelParams, 0, 0);

		// Cull texture pass prep
		Context->VSSetShader(ModelVS, 0, 0);
		Context->PSSetShader(ModelPS, 0, 0);
		Context->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
		Context->VSSetShaderResources(0, 1, &VertexBufferView);
		Context->VSSetConstantBuffers(0, 1, &ModelParamsBuffer);

		// Front
    	Context->OMSetRenderTargets(1, &FrontRTV, nullptr);
		Context->RSSetState(CullFront);
		Context->DrawIndexed(36, 0, 0);

		// Back
    	Context->OMSetRenderTargets(1, &BackRTV, nullptr);
		Context->RSSetState(CullBack);
		Context->DrawIndexed(36, 0, 0);

		// Raymarch volume
    	Context->OMSetRenderTargets(1, &BackbufferRTV, BackbufferDSV);
		Context->RSSetState(CullNone);
		Context->VSSetShader(RaymarchVS, 0, 0);
		Context->PSSetShader(RaymarchPS, 0, 0);
		Context->VSSetConstantBuffers(0, 1, &ModelParamsBuffer);
		Context->PSSetConstantBuffers(0, 1, &RaymarchParamsBuffer);
		Context->PSSetShaderResources(0, 1, &VolumeSRV);
		Context->PSSetShaderResources(1, 1, &FrontSRV);
		Context->PSSetShaderResources(2, 1, &BackSRV);
		Context->PSSetShaderResources(3, 1, &ColormapSRV);
		Context->PSSetSamplers(0, 1, &LinearSampler);
		Context->DrawIndexed(36, 0, 0);
		Context->PSSetShaderResources(0, 8, NullSRV);

		// Lamp
		ModelParams.World = Mat4Translate(RaymarchParams.LightPos) * Mat4Scale(0.25f);
		Context->UpdateSubresource(ModelParamsBuffer, 0, 0, &ModelParams, 0, 0);
		Context->VSSetShader(LampVS, 0, 0);
		Context->PSSetShader(LampPS, 0, 0);
		Context->VSSetConstantBuffers(0, 1, &ModelParamsBuffer);
		Context->DrawIndexed(36, 0, 0);

		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		ImGui::EndFrame();

		SwapChain->Present(0, 0);
	}

	return (0);
}

void		
ProcessInput(GLFWwindow *Window)
{
	f32     CamSpeed = gDeltaTime * 2.5f;

    if (glfwGetKey(Window, GLFW_KEY_ESCAPE) == GLFW_TRUE)
    {
        glfwSetWindowShouldClose(Window, GLFW_TRUE);
    }

    if (glfwGetKey(Window, GLFW_KEY_W))
    {
        gCamera.Pos = gCamera.Pos + CamSpeed * gCamera.Front;
    }
    if (glfwGetKey(Window, GLFW_KEY_S))
    {
        gCamera.Pos = gCamera.Pos - CamSpeed * gCamera.Front;
    }
    if (glfwGetKey(Window, GLFW_KEY_A))
    {
        v3 Offset = CamSpeed * Normalize(Cross(gCamera.Front, gCamera.Up));
        gCamera.Pos = gCamera.Pos + Offset;
    }
    if (glfwGetKey(Window, GLFW_KEY_D))
    {
        v3 Offset = CamSpeed * Normalize(Cross(gCamera.Front, gCamera.Up));
        gCamera.Pos = gCamera.Pos - Offset;
    }
	if (glfwGetKey(Window, GLFW_KEY_Q) == GLFW_PRESS)
    {
        glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        gImGuiControl = true;
    }
    if (glfwGetKey(Window, GLFW_KEY_E) == GLFW_PRESS)
    {
        glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        gImGuiControl = false;
    }
}

void
MouseCallback(GLFWwindow *Window,
			  f64 XPos,
			  f64 YPos)
{
	f64             XOffset,
                    YOffset;
    const f64       MouseSens = 0.08f;
    v3          	Direction;
	static f64 		Yaw = -90.0f;
   	static f64 		Pitch = 0.0f;
   	static f64 		LastX = SCR_WIDTH / 2;
   	static f64 		LastY = SCR_HEIGHT / 2;


	if (!gImGuiControl)
	{
		if (gFirstMouse)
		{
			LastX = XPos;
			LastY = YPos;
			gFirstMouse = FALSE;
		}

		XOffset = XPos - LastX,
		YOffset = LastY - YPos;
		LastX = XPos;
		LastY = YPos;

		XOffset *= -MouseSens;
		YOffset *= MouseSens;

		Yaw += XOffset;
		Pitch += YOffset;

		if (Pitch > 89.0f)
		{
			Pitch = 89.0f;
		}
		if (Pitch < -89.0f)
		{
			Pitch = -89.0f;
		}

		Direction.x = Cos(DegsToRads((f32)Yaw)) * Cos(DegsToRads((f32)Pitch));
		Direction.y = Sin(DegsToRads((f32)Pitch));
		Direction.z = Sin(DegsToRads((f32)Yaw)) * Cos(DegsToRads((f32)Pitch));

		gCamera.Front = Normalize(Direction);
	}
	else
	{
		gFirstMouse = TRUE;
	}
}


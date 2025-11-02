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

#define PROBE_COUNT_X		32
#define PROBE_COUNT_Y		32
#define PROBE_COUNT_Z		32
#define PROBE_COUNT_TOTAL	PROBE_COUNT_X * PROBE_COUNT_Y * PROBE_COUNT_Z

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
	f32		Absorption;
	f32		DensityScale;
	b32 	UseProbes;
	f32		_Pad0[2];
};

struct probe
{
	v3		Position;
	f32		Transmittance;
};

struct grid_params
{
	v3i		GridDims;
	u32		ProbeCount;

	v3		GridMin;
	f32		_Pad0;

	v3		GridMax;
	f32		_Pad1;

	v3		GridExtents;
	f32		_Pad2;

	v3		GridExtentsRcp;
	f32		_Pad3;

	v3		CellSize;
	f32		_Pad4;
};

camera gCamera;
f32 gDeltaTime = 0;
f32 gLastFrame = 0;
b32 gFirstMouse = TRUE;
b32 gImGuiControl = FALSE;

ID3D11ShaderResourceView		*NULL_SRV[8] = {};
ID3D11UnorderedAccessView		*NULL_UAV[8] = {};

void		ProcessInput(GLFWwindow *Window);
void		MouseCallback(GLFWwindow *Window, f64 XPos, f64 YPos);
f32			Clamp(f32 x, f32 Low, f32 High);
f32        	Cap(s32 X, s32 Size, f32 Width, f32 Mid, f32 Peak);
void		GenerateSphereData(std::vector<v3> &Vertices, std::vector<u32> &Indices, s32 SectorCount, s32 StackCount, f32 Radius);

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
							*LampVS,
							*ProbeDebugVS;
	ID3D11PixelShader 		*ModelPS,
							*RaymarchPS,
							*LampPS,
							*ProbeDebugPS;
	ID3D11ComputeShader		*ProbeCS;
	ID3DBlob 				*ModelVSBlob,
			 				*ModelPSBlob,
							*RaymarchVSBlob,
			 				*RaymarchPSBlob,
							*LampVSBlob,
			 				*LampPSBlob,
							*ProbeDebugVSBlob,
			 				*ProbeDebugPSBlob,
							*ProbeCSBlob;


	Hr = D3DReadFileToBlob(L"build/model_vs.cso", &ModelVSBlob);
	Hr = D3DReadFileToBlob(L"build/model_ps.cso", &ModelPSBlob);
	Hr = D3DReadFileToBlob(L"build/raymarch_vs.cso", &RaymarchVSBlob);
	Hr = D3DReadFileToBlob(L"build/raymarch_ps.cso", &RaymarchPSBlob);
	Hr = D3DReadFileToBlob(L"build/lamp_vs.cso", &LampVSBlob);
	Hr = D3DReadFileToBlob(L"build/lamp_ps.cso", &LampPSBlob);
	Hr = D3DReadFileToBlob(L"build/probe_debug_vs.cso", &ProbeDebugVSBlob);
	Hr = D3DReadFileToBlob(L"build/probe_debug_ps.cso", &ProbeDebugPSBlob);
	Hr = D3DReadFileToBlob(L"build/probe_cs.cso", &ProbeCSBlob);

	Device->CreateVertexShader(ModelVSBlob->GetBufferPointer(), ModelVSBlob->GetBufferSize(), NULL, &ModelVS);
	Device->CreatePixelShader(ModelPSBlob->GetBufferPointer(), ModelPSBlob->GetBufferSize(), NULL, &ModelPS);
	Device->CreateVertexShader(RaymarchVSBlob->GetBufferPointer(), RaymarchVSBlob->GetBufferSize(), NULL, &RaymarchVS);
	Device->CreatePixelShader(RaymarchPSBlob->GetBufferPointer(), RaymarchPSBlob->GetBufferSize(), NULL, &RaymarchPS);
	Device->CreateVertexShader(LampVSBlob->GetBufferPointer(), LampVSBlob->GetBufferSize(), NULL, &LampVS);
	Device->CreatePixelShader(LampPSBlob->GetBufferPointer(), LampPSBlob->GetBufferSize(), NULL, &LampPS);
	Device->CreateVertexShader(ProbeDebugVSBlob->GetBufferPointer(), ProbeDebugVSBlob->GetBufferSize(), NULL, &ProbeDebugVS);
	Device->CreatePixelShader(ProbeDebugPSBlob->GetBufferPointer(), ProbeDebugPSBlob->GetBufferSize(), NULL, &ProbeDebugPS);
	Device->CreateComputeShader(ProbeCSBlob->GetBufferPointer(), ProbeCSBlob->GetBufferSize(), NULL, &ProbeCS);

	//////////////////////////////////////////////////////////////////////////
	// Cube buffer setup

	ID3D11Buffer						*CubeVertexBuffer,
										*CubeIndexBuffer;
	ID3D11ShaderResourceView			*CubeVertexBufferView;
	D3D11_BUFFER_DESC					CubeVertexBufferDesc = {},
										CubeIndexBufferDesc = {};
	D3D11_SUBRESOURCE_DATA				CubeVertexBufferSubData = {},
										CubeIndexBufferSubData = {};
	D3D11_SHADER_RESOURCE_VIEW_DESC		CubeVertexBufferViewDesc;


	CubeVertexBufferDesc.ByteWidth = sizeof(CubeVertices);
	CubeVertexBufferDesc.StructureByteStride = sizeof(v3);
	CubeVertexBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	CubeVertexBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	CubeVertexBufferSubData.pSysMem = CubeVertices;

	CubeIndexBufferDesc.ByteWidth = sizeof(CubeIndices);
	CubeIndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	CubeIndexBufferSubData.pSysMem = CubeIndices;

	Hr = Device->CreateBuffer(&CubeVertexBufferDesc, &CubeVertexBufferSubData, &CubeVertexBuffer);
	Hr = Device->CreateBuffer(&CubeIndexBufferDesc, &CubeIndexBufferSubData, &CubeIndexBuffer);

	CubeVertexBufferViewDesc.Format = DXGI_FORMAT_UNKNOWN;
	CubeVertexBufferViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	CubeVertexBufferViewDesc.Buffer.FirstElement = 0;
	CubeVertexBufferViewDesc.Buffer.NumElements = 8;

	Hr = Device->CreateShaderResourceView(CubeVertexBuffer, &CubeVertexBufferViewDesc, &CubeVertexBufferView);

	//////////////////////////////////////////////////////////////////////////
	// Sphere buffer setup

	ID3D11Buffer						*SphereVertexBuffer,
										*SphereIndexBuffer;
	ID3D11ShaderResourceView			*SphereVertexBufferView;
	D3D11_BUFFER_DESC					SphereVertexBufferDesc = {},
										SphereIndexBufferDesc = {};
	D3D11_SUBRESOURCE_DATA				SphereVertexBufferSubData = {},
										SphereIndexBufferSubData = {};
	D3D11_SHADER_RESOURCE_VIEW_DESC		SphereVertexBufferViewDesc;
	std::vector<v3>						SphereVertices;
	std::vector<u32>					SphereIndices;


	GenerateSphereData(SphereVertices, SphereIndices, 36, 18, 0.25f);

	SphereVertexBufferDesc.ByteWidth = sizeof(v3) * SphereVertices.size();
	SphereVertexBufferDesc.StructureByteStride = sizeof(v3);
	SphereVertexBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	SphereVertexBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	SphereVertexBufferSubData.pSysMem = SphereVertices.data();

	SphereIndexBufferDesc.ByteWidth = sizeof(u32) * SphereIndices.size();
	SphereIndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	SphereIndexBufferSubData.pSysMem = SphereIndices.data();

	Hr = Device->CreateBuffer(&SphereVertexBufferDesc, &SphereVertexBufferSubData, &SphereVertexBuffer);
	Hr = Device->CreateBuffer(&SphereIndexBufferDesc, &SphereIndexBufferSubData, &SphereIndexBuffer);

	SphereVertexBufferViewDesc.Format = DXGI_FORMAT_UNKNOWN;
	SphereVertexBufferViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SphereVertexBufferViewDesc.Buffer.FirstElement = 0;
	SphereVertexBufferViewDesc.Buffer.NumElements = SphereVertices.size();

	Hr = Device->CreateShaderResourceView(SphereVertexBuffer, &SphereVertexBufferViewDesc, &SphereVertexBufferView);

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
    // Transfer function

    ID3D11Texture1D                     *TransferFunction;
    ID3D11ShaderResourceView            *TransferFunctionSRV;
    D3D11_TEXTURE1D_DESC                TransferFunctionDesc = {};
	D3D11_SUBRESOURCE_DATA				TransferFunctionSubData = {};
    D3D11_SHADER_RESOURCE_VIEW_DESC     TransferFunctionSRVDesc = {};
    f32                                 TfMid = 0.5f,
                                        TfPeak = 1.0f,
                                        TfWidth = 1.0f;
 	s32                                 TfSize = 128;
    std::vector<f32>                    TfData = std::vector<f32>(TfSize);


	for (s32 i = 0; i < TfSize; i++)
	{
    	TfData[i] = Cap(i, TfSize, TfWidth, TfMid, TfPeak);
	}

    TransferFunctionDesc.Width = 128;
    TransferFunctionDesc.Format = DXGI_FORMAT_R32_FLOAT;
    TransferFunctionDesc.MipLevels = 1;
    TransferFunctionDesc.ArraySize = 1;
    TransferFunctionDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	TransferFunctionSubData.pSysMem = TfData.data();
	TransferFunctionSubData.SysMemPitch = TransferFunctionDesc.Width * sizeof(f32);

    TransferFunctionSRVDesc.Format = TransferFunctionDesc.Format;
    TransferFunctionSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
    TransferFunctionSRVDesc.Texture1D.MipLevels = 1;
    TransferFunctionSRVDesc.Texture1D.MostDetailedMip = 0;

    Device->CreateTexture1D(&TransferFunctionDesc, &TransferFunctionSubData, &TransferFunction);
    Device->CreateShaderResourceView(TransferFunction, &TransferFunctionSRVDesc, &TransferFunctionSRV);

	//////////////////////////////////////////////////////////////////////////
	// Volume texture

	ID3D11Texture3D						*Volume;
	ID3D11ShaderResourceView			*VolumeSRV;
	D3D11_TEXTURE3D_DESC				VolumeDesc = {};
	D3D11_SHADER_RESOURCE_VIEW_DESC		VolumeSRVDesc = {};
	D3D11_SUBRESOURCE_DATA				VolumeSubData = {};
	f32									*VolumeData;
	std::vector<int>					P;
	f32									MinNoise =  10000.0f,
										MaxNoise = -10000.0f;


	P = get_permutation_vector();
	VolumeData = (f32 *)HeapAlloc(GetProcessHeap(), 0, VOLUME_SIZE * sizeof(*VolumeData));

	for (u32 z = 0; z < VOLUME_DEPTH; z++)
	{
		for (u32 y = 0; y < VOLUME_HEIGHT; y++)
		{
			for (u32 x = 0; x < VOLUME_WIDTH; x++)
			{
				f32 Noise = 0;
				f32 Amp = 16;
				f32 Freq = 1.5f;
				u32 Octaves = 1;

				for (u32 i = 0; i < Octaves; i++)
				{
					f32 SampleX = ((2.0f * x / (f32)VOLUME_WIDTH) - 1.0f) * Freq;
					f32 SampleY = ((2.0f * y / (f32)VOLUME_HEIGHT) - 1.0f) * Freq;
					f32 SampleZ = ((2.0f * z / (f32)VOLUME_DEPTH) - 1.0f) * Freq;

					Noise += fabs(perlin(SampleX, SampleY, SampleZ, P) * Amp);

					Freq *= 2.0f;
					Amp *= 0.5f;
				}

				u32 Index = (z * VOLUME_HEIGHT * VOLUME_WIDTH) + y * VOLUME_WIDTH + x;

				if (Noise < MinNoise)
				{
					MinNoise = Noise;
				}
				if (Noise > MaxNoise)
				{
					MaxNoise = Noise;
				}

				VolumeData[Index] = Noise;
			}
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
	RaymarchParams.Absorption = 1.0;
	RaymarchParams.DensityScale = 1.0;
	RaymarchParams.UseProbes = FALSE;

	//////////////////////////////////////////////////////////////////////////
	// Grid params

	grid_params					GridParams;
	ID3D11Buffer				*GridParamsBuffer;
	D3D11_BUFFER_DESC			GridParamsBufferDesc = {};
	D3D11_SUBRESOURCE_DATA		GridParamsSubData = {};


	GridParams.GridDims = v3i(PROBE_COUNT_X, PROBE_COUNT_Y, PROBE_COUNT_Z);
	GridParams.ProbeCount = PROBE_COUNT_TOTAL;
	GridParams.GridMin = v3(0, 0, 0);
	GridParams.GridMax = v3(1, 1, 1);
	GridParams.GridExtents = GridParams.GridMax - GridParams.GridMin;
	GridParams.GridExtentsRcp.x = 1.f / GridParams.GridExtents.x;
	GridParams.GridExtentsRcp.y = 1.f / GridParams.GridExtents.y;
	GridParams.GridExtentsRcp.z = 1.f / GridParams.GridExtents.z;
	GridParams.CellSize.x = GridParams.GridExtents.x / (GridParams.GridDims.x - 1);
	GridParams.CellSize.y = GridParams.GridExtents.y / (GridParams.GridDims.y - 1);
	GridParams.CellSize.z = GridParams.GridExtents.z / (GridParams.GridDims.z - 1);

	GridParamsBufferDesc.ByteWidth = sizeof(GridParams);
	GridParamsBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	GridParamsSubData.pSysMem = &GridParams;

	Device->CreateBuffer(&GridParamsBufferDesc, &GridParamsSubData, &GridParamsBuffer);

	//////////////////////////////////////////////////////////////////////////
    // Light probes

	ID3D11Buffer							*ProbesBuffer;
	ID3D11ShaderResourceView				*ProbesSRV;
	ID3D11UnorderedAccessView				*ProbesUAV;
	D3D11_BUFFER_DESC						ProbesBufferDesc = {};
	D3D11_SHADER_RESOURCE_VIEW_DESC			ProbesSRVDesc = {};
	D3D11_UNORDERED_ACCESS_VIEW_DESC		ProbesUAVDesc = {};


	ProbesBufferDesc.ByteWidth = PROBE_COUNT_TOTAL * sizeof(probe); // TODO(matthew): hardcoded for now
	ProbesBufferDesc.StructureByteStride = sizeof(probe);
	ProbesBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	ProbesBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	ProbesSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	ProbesSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	ProbesSRVDesc.Buffer.FirstElement = 0;
	ProbesSRVDesc.Buffer.NumElements = PROBE_COUNT_TOTAL;

	ProbesUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	ProbesUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	ProbesUAVDesc.Buffer.FirstElement = 0;
	ProbesUAVDesc.Buffer.NumElements = PROBE_COUNT_TOTAL;

	Device->CreateBuffer(&ProbesBufferDesc, nullptr, &ProbesBuffer);
	Device->CreateShaderResourceView(ProbesBuffer, &ProbesSRVDesc, &ProbesSRV);
	Device->CreateUnorderedAccessView(ProbesBuffer, &ProbesUAVDesc, &ProbesUAV);

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

	f32 Time = 0;
	bool ShowProbes = false;
	s32 UpdatePerfCounter = 0;
	f32 MsPerFrame = 0;

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

		ImGui::Begin("Performance");
			if (UpdatePerfCounter % 50 == 0)
			{
				MsPerFrame = gDeltaTime * 1000;
				UpdatePerfCounter = 0;
			}
			ImGui::Text("Frametime: %f ms", MsPerFrame);
			ImGui::Text("FPS: %f", 1 / gDeltaTime);
		ImGui::End();
		UpdatePerfCounter += 1;

		ImGui::Begin("Controls");
			ImGui::DragFloat("Light X", &RaymarchParams.LightPos.x, 0.01f, -5, 5);
			ImGui::DragFloat("Light Y", &RaymarchParams.LightPos.y, 0.01f, -5, 5);
			ImGui::DragFloat("Light Z", &RaymarchParams.LightPos.z, 0.01f, -5, 5);
			ImGui::DragFloat("Absorption", &RaymarchParams.Absorption, 0.01f, 0, 5);
			ImGui::DragFloat("Density scale", &RaymarchParams.DensityScale, 0.01f, 0, 10);
			ImGui::SliderInt("Use probes", &RaymarchParams.UseProbes, 0, 1);
			ImGui::Checkbox("Show probes", &ShowProbes);
		ImGui::End();
		Context->UpdateSubresource(RaymarchParamsBuffer, 0, 0, &RaymarchParams, 0, 0);

		static f32 ClearColor[4] = { 0, 0, 0, 1 };

		Context->ClearDepthStencilView(BackbufferDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		Context->ClearRenderTargetView(BackbufferRTV, ClearColor);
		Context->ClearRenderTargetView(FrontRTV, ClearColor);
		Context->ClearRenderTargetView(BackRTV, ClearColor);
		Context->RSSetViewports(1, &Viewport);
		Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		ModelParams.World = Mat4Identity();//Mat4Rotate(Time, v3(0, 1, 0)) * Mat4Translate(v3(-0.5f, -0.5f, -0.5f));
		ModelParams.View = Mat4LookAtLH(gCamera.Pos, gCamera.Pos + gCamera.Front, gCamera.Up);
		ModelParams.Proj = Mat4PerspectiveLH(45.0f, (f32)SCR_WIDTH / (f32)SCR_HEIGHT, 0.1f, 1000.0f);
		Context->UpdateSubresource(ModelParamsBuffer, 0, 0, &ModelParams, 0, 0);

		// Cull texture pass prep
		Context->VSSetShader(ModelVS, 0, 0);
		Context->PSSetShader(ModelPS, 0, 0);
		Context->IASetIndexBuffer(CubeIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
		Context->VSSetShaderResources(0, 1, &CubeVertexBufferView);
		Context->VSSetConstantBuffers(0, 1, &ModelParamsBuffer);

		// Front-face cull to get back positions
        Context->OMSetRenderTargets(1, &BackRTV, nullptr);
        Context->RSSetState(CullFront);
        Context->DrawIndexed(36, 0, 0);

        // Back-face cull to get front positions
        Context->OMSetRenderTargets(1, &FrontRTV, nullptr);
        Context->RSSetState(CullBack);
        Context->DrawIndexed(36, 0, 0);

    	Context->OMSetRenderTargets(1, &BackbufferRTV, BackbufferDSV);
		Context->RSSetState(CullNone);
		// Probes compute pass
		Context->CSSetShader(ProbeCS, 0, 0);
		Context->CSSetSamplers(0, 1, &LinearSampler);
		Context->CSSetConstantBuffers(0, 1, &ModelParamsBuffer);
		Context->CSSetConstantBuffers(1, 1, &RaymarchParamsBuffer);
		Context->CSSetConstantBuffers(2, 1, &GridParamsBuffer);
        Context->CSSetShaderResources(0, 1, &VolumeSRV);
		Context->CSSetUnorderedAccessViews(0, 1, &ProbesUAV, 0);
		Context->Dispatch(GridParams.GridDims.x, GridParams.GridDims.y, GridParams.GridDims.z);
		Context->CSSetUnorderedAccessViews(0, 8, NULL_UAV, 0);	

		if (ShowProbes)
		{
			Context->VSSetShader(ProbeDebugVS, 0, 0);
			Context->PSSetShader(ProbeDebugPS, 0, 0);
			Context->VSSetConstantBuffers(0, 1, &ModelParamsBuffer);
			Context->VSSetConstantBuffers(1, 1, &GridParamsBuffer);
			Context->VSSetShaderResources(1, 1, &ProbesSRV);
			Context->DrawIndexedInstanced(36, GridParams.ProbeCount, 0, 0, 0);
			Context->VSSetShaderResources(0, 8, NULL_SRV);
			Context->PSSetShaderResources(0, 8, NULL_SRV);
		}

		// Raymarch volume
		Context->VSSetShader(RaymarchVS, 0, 0);
		Context->PSSetShader(RaymarchPS, 0, 0);
		Context->VSSetConstantBuffers(0, 1, &ModelParamsBuffer);
		Context->PSSetConstantBuffers(0, 1, &ModelParamsBuffer);
		Context->PSSetConstantBuffers(1, 1, &RaymarchParamsBuffer);
		Context->PSSetConstantBuffers(2, 1, &GridParamsBuffer);
		Context->PSSetShaderResources(0, 1, &VolumeSRV);
		Context->PSSetShaderResources(1, 1, &FrontSRV);
		Context->PSSetShaderResources(2, 1, &BackSRV);
		Context->PSSetShaderResources(3, 1, &ColormapSRV);
		Context->PSSetShaderResources(4, 1, &ProbesSRV);
		/* Context->PSSetShaderResources(4, 1, &TransferFunctionSRV); */
		Context->PSSetSamplers(0, 1, &LinearSampler);
		Context->DrawIndexed(36, 0, 0);
		Context->PSSetShaderResources(0, 8, NULL_SRV);

		// Lamp
		ModelParams.World = Mat4Translate(RaymarchParams.LightPos);
		Context->UpdateSubresource(ModelParamsBuffer, 0, 0, &ModelParams, 0, 0);
		Context->IASetIndexBuffer(SphereIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
		Context->VSSetShaderResources(0, 1, &SphereVertexBufferView);
		Context->VSSetShader(LampVS, 0, 0);
		Context->PSSetShader(LampPS, 0, 0);
		Context->VSSetConstantBuffers(0, 1, &ModelParamsBuffer);
		Context->DrawIndexed(SphereIndices.size(), 0, 0);

		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		ImGui::EndFrame();

		SwapChain->Present(0, 0);
		Time += 0.05f;
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

/* Single-cap transfer function with variable width and midpoint

   Peak -- |         /\
           |        /  \
           |       /    \
           |      /      \
           |     /        \
           |    /          \
           |   /            \
   0.0 -- ----|------|-------|----
           |  L     Mid      R

*/
f32
Cap(s32 X, s32 Size, f32 Width, f32 Mid, f32 Peak)
{
    s32     Midpoint = s32(Size * Mid);
    s32     Offset = s32(Size * Width / 2);
    s32     Left = Midpoint - Offset,
            Right = Midpoint + Offset;
    f32     Ret;


    if (X < Midpoint)
    {
        Ret = f32(X - Left) / (Midpoint - Left);
    }
    else
    {
        Ret = f32(Right - X) / (Midpoint - Left);
    }

    return (Clamp(Ret * Peak, 0, 1));
}

f32
Clamp(f32 x, f32 Low, f32 High)
{
	if (x < Low)
	{
		return (Low);
	}
	else if (x > High)
	{
		return (High);
	}
	else
	{
		return (x);
	}
}

void
GenerateSphereData(std::vector<v3> &Vertices,
				   std::vector<u32> &Indices,
				   s32 SectorCount,
				   s32 StackCount,
				   f32 Radius)
{
	// clear memory of prev arrays
	std::vector<v3>().swap(Vertices);
	std::vector<u32>().swap(Indices);

	f32 x, y, z, xy;                              // vertex position

	f32 SectorStep = 2 * PI / SectorCount;
	f32 StackStep = PI / StackCount;
	f32 SectorAngle, StackAngle;

	for(s32 i = 0; i <= StackCount; ++i)
	{
		StackAngle = PI / 2 - i * StackStep;        // starting from pi/2 to -pi/2
		xy = Radius * Cos(StackAngle);             // r * cos(u)
		z = Radius * Sin(StackAngle);              // r * sin(u)

		// add (sectorCount+1) vertices per stack
		// first and last vertices have same position and normal, but different tex coords
		for(s32 j = 0; j <= SectorCount; ++j)
		{
			SectorAngle = j * SectorStep;           // starting from 0 to 2pi

			// vertex position (x, y, z)
			x = xy * Cos(SectorAngle);             // r * cos(u) * cos(v)
			y = xy * Sin(SectorAngle);             // r * cos(u) * sin(v)
			Vertices.push_back(v3(x, y, z));
		}
	}

	u32 k1, k2;
	for(int i = 0; i < StackCount; ++i)
	{
		k1 = i * (SectorCount + 1);     // beginning of current stack
		k2 = k1 + SectorCount + 1;      // beginning of next stack

		for(int j = 0; j < SectorCount; ++j, ++k1, ++k2)
		{
			// 2 triangles per sector excluding first and last stacks
			// k1 => k2 => k1+1
			if(i != 0)
			{
				Indices.push_back(k1);
				Indices.push_back(k2);
				Indices.push_back(k1 + 1);
			}

			// k1+1 => k2 => k2+1
			if(i != (StackCount - 1))
			{
				Indices.push_back(k1 + 1);
				Indices.push_back(k2);
				Indices.push_back(k2 + 1);
			}
		}
	}
}


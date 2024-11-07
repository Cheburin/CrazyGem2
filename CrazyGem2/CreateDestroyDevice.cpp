#include "main.h"

#include "DXUTgui.h"
#include "SDKmisc.h"

HWND DXUTgetWindow();

GraphicResources * G;

SceneState scene_state;


std::unique_ptr<Keyboard> _keyboard;
std::unique_ptr<Mouse> _mouse;

CDXUTDialogResourceManager          g_DialogResourceManager;
CDXUTTextHelper*                    g_pTxtHelper = NULL;

#include <codecvt>

inline float lerp(float x1, float x2, float t){
	return x1*(1.0 - t) + x2*t;
}

inline float nextFloat(float x1, float x2){
	return lerp(x1, x2, (float)std::rand() / (float)RAND_MAX);
}
///////////////////////////////////////////////////////////////
void FillGrid_Indexed(std::vector<VertexPositionNormalTangentColorTexture> & _vertices, std::vector<UINT> & _indices, DWORD dwWidth, DWORD dwLength,
	_In_opt_ std::function<float __cdecl(SimpleMath::Vector3)> setHeight)
{
	// Fill vertex buffer
	for (DWORD i = 0; i <= dwLength; ++i)
	{
		for (DWORD j = 0; j <= dwWidth; ++j)
		{
			VertexPositionNormalTangentColorTexture    pVertex;
			pVertex.position.x = ((float)j / dwWidth);
			pVertex.position.z = ((float)i / dwLength);
			pVertex.position.y = setHeight(pVertex.position);
			_vertices.push_back(pVertex);
		}
	}
	VertexPositionNormalTangentColorTexture    pVertex;

	// Fill index buffer
	int index = 0;
	for (DWORD i = 0; i < dwLength; ++i)
	{
		for (DWORD j = 0; j < dwWidth; ++j)
		{
			_indices.push_back((DWORD)(i     * (dwWidth + 1) + j));
			_indices.push_back((DWORD)((i + 1) * (dwWidth + 1) + j));
			_indices.push_back((DWORD)(i     * (dwWidth + 1) + j + 1));

			_indices.push_back((DWORD)(i     * (dwWidth + 1) + j + 1));
			_indices.push_back((DWORD)((i + 1) * (dwWidth + 1) + j));
			_indices.push_back((DWORD)((i + 1) * (dwWidth + 1) + j + 1));
		}
	}
}

///////////////////////////////////////////////////////////////
DirectX::XMFLOAT4X4    envViews[6];
DirectX::XMFLOAT4X4    envProj;
///////////////////////////////////////////////////////////////

HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* device, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
	void* pUserContext)
{
	std::srand(unsigned(std::time(0)));

	HRESULT hr;

	ID3D11DeviceContext* context = DXUTGetD3D11DeviceContext();

	G = new GraphicResources();
	G->render_states = std::make_unique<CommonStates>(device);
	G->scene_constant_buffer = std::make_unique<ConstantBuffer<SceneState> >(device);
	G->ray_constant_buffer = std::make_unique<ConstantBuffer<RayState> >(device);

	_keyboard = std::make_unique<Keyboard>();
	_mouse = std::make_unique<Mouse>();
	HWND hwnd = DXUTgetWindow();
	_mouse->SetWindow(hwnd);

	g_DialogResourceManager.OnD3D11CreateDevice(device, context);
	g_pTxtHelper = new CDXUTTextHelper(device, context, &g_DialogResourceManager, 15);

	//effects
	{
		std::map<const WCHAR*, EffectShaderFileDef> shaderDef;
		shaderDef[L"VS"] = { L"ModelVS.hlsl", L"GROUND_VS", L"vs_5_0" };
		shaderDef[L"PS"] = { L"ModelPS.hlsl", L"GROUND_PS", L"ps_5_0" };

		G->ground_effect = createHlslEffect(device, shaderDef);
	}
	{
		std::map<const WCHAR*, EffectShaderFileDef> shaderDef;
		shaderDef[L"VS"] = { L"ModelVS.hlsl", L"SKY_VS", L"vs_5_0" };
		shaderDef[L"PS"] = { L"ModelPS.hlsl", L"SKY_PS", L"ps_5_0" };

		G->sky_effect = createHlslEffect(device, shaderDef);
	}
	{
		std::map<const WCHAR*, EffectShaderFileDef> shaderDef;
		shaderDef[L"VS"] = { L"ModelVS.hlsl", L"BOX_VS", L"vs_5_0" };
		shaderDef[L"GS"] = { L"ModelVS.hlsl", L"BOX_GS", L"gs_5_0" };
		shaderDef[L"PS"] = { L"ModelPS.hlsl", L"BOX_PS", L"ps_5_0" };

		G->box_effect = createHlslEffect(device, shaderDef);
	}
	{
		std::map<const WCHAR*, EffectShaderFileDef> shaderDef;
		shaderDef[L"VS"] = { L"ModelVS.hlsl", L"FIGURE_VS", L"vs_5_0" };
		shaderDef[L"PS"] = { L"ModelPS.hlsl", L"FIGURE_PS", L"ps_5_0" };

		G->figure_effect = createHlslEffect(device, shaderDef);
	}
	{
		std::map<const WCHAR*, EffectShaderFileDef> shaderDef;
		shaderDef[L"VS"] = { L"ModelVS.hlsl", L"FIGURE_VS", L"vs_5_0" };
		shaderDef[L"PS"] = { L"ModelPS.hlsl", L"BENCH_PS", L"ps_5_0" };

		G->bench_effect = createHlslEffect(device, shaderDef);
	}
	{
		std::map<const WCHAR*, EffectShaderFileDef> shaderDef;
		shaderDef[L"VS"] = { L"ModelVS.hlsl", L"RAY_VS", L"vs_5_0" };
		shaderDef[L"GS"] = { L"ModelVS.hlsl", L"RAY_GS", L"gs_5_0" };
		shaderDef[L"PS"] = { L"ModelPS.hlsl", L"RAY_PS", L"ps_5_0" };

		G->ray_effect = createHlslEffect(device, shaderDef);
	}
	{
		std::map<const WCHAR*, EffectShaderFileDef> shaderDef;
		shaderDef[L"VS"] = { L"ModelVS.hlsl", L"SPHERE_VS", L"vs_5_0" };
		shaderDef[L"PS"] = { L"ModelPS.hlsl", L"RAY_PS", L"ps_5_0" };

		G->sphere_hit_effect = createHlslEffect(device, shaderDef);
	}
	{
		std::map<const WCHAR*, EffectShaderFileDef> shaderDef;
		shaderDef[L"VS"] = { L"ModelVS.hlsl", L"SPHERE_VS", L"vs_5_0" };
		shaderDef[L"PS"] = { L"ModelPS.hlsl", L"BOX_PS", L"ps_5_0" };

		G->sphere_effect = createHlslEffect(device, shaderDef);
	}
	{
		std::map<const WCHAR*, EffectShaderFileDef> shaderDef;
		shaderDef[L"VS"] = { L"ModelVS.hlsl", L"RENDER_TBN_VS", L"vs_5_0" };
		shaderDef[L"GS"] = { L"ModelVS.hlsl", L"RENDER_TBN_GS", L"gs_5_0" };
		shaderDef[L"PS"] = { L"ModelPS.hlsl", L"RENDER_TBN_PS", L"ps_5_0" };

		G->tbn_effect = createHlslEffect(device, shaderDef);
	}

	//models
	{
		G->ground_model = CreateModelMeshPart(device, [=](std::vector<VertexPositionNormalTangentColorTexture> & _vertices, std::vector<UINT> & _indices){
			FillGrid_Indexed(_vertices, _indices, 1, 1, [=](SimpleMath::Vector3 p){
				return 0;
			});
		});

		G->figure_model = CreateModelMeshPart(device, [=](std::vector<VertexPositionNormalTangentColorTexture> & _vertices, std::vector<UINT> & _indices){
			loadModelFromFile("Models\\Figurine_09_fbx.FBX", _vertices, _indices);
			CreateBoundingVolumes(_vertices, &(G->boundingVolumes["figure"]));
		});

		G->bench_model = CreateModelMeshPart(device, [=](std::vector<VertexPositionNormalTangentColorTexture> & _vertices, std::vector<UINT> & _indices){
			loadModelFromFile("Models\\Bench.FBX", _vertices, _indices);
			CreateBoundingVolumes(_vertices, &(G->boundingVolumes["bench"]));
		});

		G->sphere_model = GeometricPrimitive::CreateSphere(context, 1, 32, false, false);

		hr = D3DX11CreateShaderResourceViewFromFile(device,  L"Media\\Textures\\meadow.dds", NULL, NULL, G->sky_cube_texture.ReleaseAndGetAddressOf(), NULL);

		hr = D3DX11CreateShaderResourceViewFromFile(device, L"Models\\Maps\\Figurine_09_norm.jpg", NULL, NULL, G->figure_normal_texture.ReleaseAndGetAddressOf(), NULL);
	}

	//layouts
	{
		G->ground_model->CreateInputLayout(device, G->ground_effect.get(), G->model_input_layout.ReleaseAndGetAddressOf());
	}

	//RStates
	{
		D3D11_BLEND_DESC desc;
		ZeroMemory(&desc, sizeof(desc));

		desc.RenderTarget[0].BlendEnable = true;

		desc.RenderTarget[0].SrcBlend = desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

		desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		HRESULT hr = device->CreateBlendState(&desc, G->alfaBlend.ReleaseAndGetAddressOf());
	}

	//envariement
	{
		D3D11_TEXTURE2D_DESC dstex;
		dstex.Height = dstex.Width = 2048;
		dstex.MipLevels = 1;
		dstex.ArraySize = 6;
		dstex.SampleDesc.Count = 1;
		dstex.SampleDesc.Quality = 0;
		dstex.Format = DXGI_FORMAT_R32_TYPELESS;
		dstex.Usage = D3D11_USAGE_DEFAULT;
		dstex.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
		dstex.CPUAccessFlags = 0;
		dstex.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
		device->CreateTexture2D(&dstex, NULL, G->envDepth_T2D.ReleaseAndGetAddressOf());

		D3D11_DEPTH_STENCIL_VIEW_DESC DescDS;
		DescDS.Flags = 0;
		DescDS.Format = DXGI_FORMAT_D32_FLOAT;
		DescDS.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		DescDS.Texture2DArray.FirstArraySlice = 0;
		DescDS.Texture2DArray.ArraySize = 6;
		DescDS.Texture2DArray.MipSlice = 0;
		DescDS.Texture2DArray.ArraySize = 1;
		for (int i = 0; i < 6; ++i)
		{
			DescDS.Texture2DArray.FirstArraySlice = i;
			device->CreateDepthStencilView(G->envDepth_T2D.Get(), &DescDS, G->envDepth_DSVs[i].ReleaseAndGetAddressOf());
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		ZeroMemory(&SRVDesc, sizeof(SRVDesc));
		SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
		SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURECUBE;
		SRVDesc.TextureCube.MipLevels = 1;
		SRVDesc.TextureCube.MostDetailedMip = 0;
		device->CreateShaderResourceView(G->envDepth_T2D.Get(), &SRVDesc, G->envDepth_SRV.ReleaseAndGetAddressOf());

		D3D11_TEXTURE2D_DESC rttex;
		rttex.Height = rttex.Width = 2048;
		rttex.MipLevels = 1;
		rttex.ArraySize = 6;
		rttex.SampleDesc.Count = 1;
		rttex.SampleDesc.Quality = 0;
		rttex.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		rttex.Usage = D3D11_USAGE_DEFAULT;
		rttex.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		rttex.CPUAccessFlags = 0;
		rttex.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
		device->CreateTexture2D(&rttex, NULL, G->envColor_T2D.ReleaseAndGetAddressOf());

		D3D11_RENDER_TARGET_VIEW_DESC DescRT;
		DescRT.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		DescRT.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		DescRT.Texture2DArray.FirstArraySlice = 0;
		DescRT.Texture2DArray.ArraySize = 6;
		DescRT.Texture2DArray.MipSlice = 0;
		DescRT.Texture2DArray.ArraySize = 1;
		for (int i = 0; i < 6; ++i)
		{
			DescRT.Texture2DArray.FirstArraySlice = i;
			device->CreateRenderTargetView(G->envColor_T2D.Get(), &DescRT, G->envColor_RTVs[i].ReleaseAndGetAddressOf());
		}
	}
	//env matrixs
	SimpleMath::Matrix envView;
	{
		envView.Right(   SimpleMath::Vector3(0, 0, -1));
		envView.Up(      SimpleMath::Vector3(0, 1, 0));
		envView.Backward(SimpleMath::Vector3(1, 0, 0));
		envViews[0] = envView.Transpose();
		envView.Right(   SimpleMath::Vector3(0, 0, 1));
		envView.Up(      SimpleMath::Vector3(0, 1, 0));
		envView.Backward(SimpleMath::Vector3(-1, 0, 0));
		envViews[1] = envView.Transpose();

		envView.Right(SimpleMath::Vector3(1, 0, 0));
		envView.Up(SimpleMath::Vector3(0, 0, -1));
		envView.Backward(SimpleMath::Vector3(0, 1, 0));
		envViews[2] = envView.Transpose();
		envView.Right(SimpleMath::Vector3(1, 0, 0));
		envView.Up(SimpleMath::Vector3(0, 0, 1));
		envView.Backward(SimpleMath::Vector3(0, -1, 0));
		envViews[3] = envView.Transpose();

		envView.Right(SimpleMath::Vector3(1, 0, 0));
		envView.Up(SimpleMath::Vector3(0, 1, 0));
		envView.Backward(SimpleMath::Vector3(0, 0, 1));
		envViews[4] = envView.Transpose();
		envView.Right(SimpleMath::Vector3(-1, 0, 0));
		envView.Up(SimpleMath::Vector3(0, 1, 0));
		envView.Backward(SimpleMath::Vector3(0, 0, -1));
		envViews[5] = envView.Transpose();

		envProj = SimpleMath::Matrix(DirectX::XMMatrixPerspectiveFovLH(D3DX_PI / 2, 1, 0.1, 1000));
	}
	return S_OK;
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice(void* pUserContext)
{
	delete g_pTxtHelper;

	g_DialogResourceManager.OnD3D11DestroyDevice();

	_mouse = 0;

	_keyboard = 0;

	delete G;
}

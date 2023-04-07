#include "AppBase.h"

// std
#include <cassert>
#include <vector>
#include <iostream>

// windows
#include <windowsx.h>

// d3d
//#include <dxgi.h>
#include <d3dcompiler.h>

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mWindow is valid
	return AppBase::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

AppBase* AppBase::mApp = nullptr;

AppBase::AppBase(HINSTANCE instance)
	: mInstance(instance)
	, mViewport()
{
	assert(mApp == nullptr);
	mApp = this;
}

AppBase::~AppBase() {}

AppBase *AppBase::GetApp()
{
	return mApp;
}

bool AppBase::Init()
{
	if (!InitWindow())
	{
		return false;
	}

	if (!InitDirect3D())
	{
		return false;
	}

	OnResize();

	// mesh
	{
		mMesh = GeometryGenerator::CreateBox(1, 1, 1);
	}

	// vertex buffer
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(GeometryGenerator::VertexData) * UINT(mMesh.vertices.size());
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = mMesh.vertices.data();
		data.SysMemPitch = 0;
		data.SysMemSlicePitch = 0;

		ThrowIfFailed(mDevice->CreateBuffer(&desc, &data, &mVertexBuffer));

		NameResource(mVertexBuffer.Get(), "AppBase_VertexBuffer");

#if 0
		{
			desc.Usage = D3D11_USAGE_STAGING;
			desc.BindFlags = 0;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

			ComPtr<ID3D11Buffer> pStagingBuffer;
			ThrowIfFailed(mDevice->CreateBuffer(&desc, &data, &pStagingBuffer));

			mContext->CopyResource(pStagingBuffer.Get(), mVertexBuffer.Get());

			D3D11_MAPPED_SUBRESOURCE mapped;
			mContext->Map(pStagingBuffer.Get(), 0, D3D11_MAP_READ, 0, &mapped);

			OutputDebugStringA("");

			mContext->Unmap(pStagingBuffer.Get(), 0);
		}
#endif
	}

	// index buffer
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(GeometryGenerator::IndexType) * UINT(mMesh.indices.size());
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = mMesh.indices.data();
		data.SysMemPitch = 0;
		data.SysMemSlicePitch = 0;

		ThrowIfFailed(mDevice->CreateBuffer(&desc, &data, &mIndexBuffer));
		
		NameResource(mIndexBuffer.Get(), "AppBase_IndexBuffer");

#if 0
		{
			desc.Usage = D3D11_USAGE_STAGING;
			desc.BindFlags = 0;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

			ComPtr<ID3D11Buffer> pStagingBuffer;
			ThrowIfFailed(mDevice->CreateBuffer(&desc, &data, &pStagingBuffer));

			mContext->CopyResource(pStagingBuffer.Get(), mIndexBuffer.Get());

			D3D11_MAPPED_SUBRESOURCE mapped;
			mContext->Map(pStagingBuffer.Get(), 0, D3D11_MAP_READ, 0, &mapped);

			OutputDebugStringA("");

			mContext->Unmap(pStagingBuffer.Get(), 0);
		}
#endif
	}

	mIndexStart = 0;
	mIndexCount = UINT(mMesh.indices.size());
	mVertexBase = 0;

	// default vertex shader
	{
		std::wstring path = L"shaders/Default.hlsl";

		ComPtr<ID3DBlob> pCode = CompileShader(path, nullptr, "DefaultVS", ShaderTarget::VS);

		ThrowIfFailed(mDevice->CreateVertexShader(pCode->GetBufferPointer(),
												  pCode->GetBufferSize(),
												  nullptr,
												  &mVertexShader));

		NameResource(mVertexShader.Get(), "DefaultVS");

		// input layout
		{
			std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			};

			ThrowIfFailed(mDevice->CreateInputLayout(desc.data(),
													 UINT(desc.size()),
													 pCode->GetBufferPointer(),
													 pCode->GetBufferSize(),
													 &mInputLayout));

			NameResource(mInputLayout.Get(), "DefaultVS_InputLayout");
		}
	}

	// default pixel shader
	{
		std::wstring path = L"shaders/Default.hlsl";

		ComPtr<ID3DBlob> pCode = CompileShader(path, nullptr, "DefaultPS", ShaderTarget::PS);

		ThrowIfFailed(mDevice->CreatePixelShader(pCode->GetBufferPointer(),
												 pCode->GetBufferSize(),
												 nullptr,
												 &mPixelShader));

		NameResource(mPixelShader.Get(), "DefaultPS");
	}

	// main pass CB
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(MainPassCB);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		ThrowIfFailed(mDevice->CreateBuffer(&desc, nullptr, &mMainPassCB));

		NameResource(mMainPassCB.Get(), "MainPassCB");
	}

	// object CB
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(ObjectCB);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		ThrowIfFailed(mDevice->CreateBuffer(&desc, nullptr, &mObjectCB));

		NameResource(mObjectCB.Get(), "ObjectCB");
	}

	// materials
	{
		Material material;
		material.diffuse = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
		material.fresnel = XMFLOAT3(0.6f, 0.6f, 0.6f);
		material.roughness = 0.2f;

		mMaterialManager.AddMaterial("default", material);
	}

	// materials structured buffer
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(Material) * UINT(mMaterialManager.GetMaterials().size());
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(Material);

		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = mMaterialManager.GetMaterials().data();
		data.SysMemPitch = 0;
		data.SysMemSlicePitch = 0;

		ThrowIfFailed(mDevice->CreateBuffer(&desc, &data, &mMaterialsSB));

		NameResource(mMaterialsSB.Get(), "MaterialsSB");

		ThrowIfFailed(mDevice->CreateShaderResourceView(mMaterialsSB.Get(), nullptr, &mMaterialsBufferSRV));

		NameResource(mMaterialsBufferSRV.Get(), "MaterialsBufferSRV");
	}

	return true;
}

bool AppBase::InitWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mInstance;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = HBRUSH(GetStockObject(NULL_BRUSH));
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	RECT rect = {0, 0, LONG(mWindowWidth), LONG(mWindowHeight)};
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;

	mWindow = CreateWindow(L"MainWnd",
						   mWindowName.c_str(),
						   WS_OVERLAPPEDWINDOW,
						   CW_USEDEFAULT,
						   CW_USEDEFAULT,
						   width,
						   height,
						   0,
						   0,
						   mInstance,
						   0);
	if (!mWindow)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(mWindow, SW_SHOW);
	UpdateWindow(mWindow);

	return true;
}

bool AppBase::InitDirect3D()
{
	ComPtr<IDXGIFactory1> pFactory; // IDXGIFactory4
	ThrowIfFailed(CreateDXGIFactory1(__uuidof(IDXGIFactory1),
									 reinterpret_cast<void**>(pFactory.GetAddressOf())));

#if 0 // list GPUs
	IDXGIAdapter1* pAdapter;
	std::vector<ComPtr<IDXGIAdapter1>> pAdapters;

	for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		pAdapters.push_back(pAdapter);
	}

	for (const auto& pAdapter : pAdapters)
	{
		DXGI_ADAPTER_DESC desc;
		ThrowIfFailed(pAdapter->GetDesc(&desc));
		
		// display current GPU name on ImGui
	}
#endif

	// GPU name
	{
		ComPtr<IDXGIAdapter1> pAdapter;
		ThrowIfFailed(pFactory->EnumAdapters1(0, &pAdapter));

		DXGI_ADAPTER_DESC desc;
		ThrowIfFailed(pAdapter->GetDesc(&desc));

		mGPUName = desc.Description;
	}

	// device & context
	{
		UINT flags = 0;
#ifdef _DEBUG
		flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // _DEBUG

		D3D_FEATURE_LEVEL featureLevel;

		ThrowIfFailed(D3D11CreateDevice(nullptr,
										D3D_DRIVER_TYPE_HARDWARE,
										nullptr,
										flags,
										nullptr,
										0,
										D3D11_SDK_VERSION,
										&mDevice,
										&featureLevel,
										&mContext));

		assert(featureLevel >= D3D_FEATURE_LEVEL_11_0);
	}

	// swap chain
	{
		DXGI_SWAP_CHAIN_DESC desc;
		desc.BufferDesc.Width = mWindowWidth;
		desc.BufferDesc.Height = mWindowHeight;
		desc.BufferDesc.RefreshRate.Numerator = 60;
		desc.BufferDesc.RefreshRate.Denominator = 1;
		desc.BufferDesc.Format = mSwapChainFormat;
		desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = min(mSwapChainBufferCount, DXGI_MAX_SWAP_CHAIN_BUFFERS);
		desc.OutputWindow = mWindow;
		desc.Windowed = true;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.Flags = mSwapChainFlags;

		ThrowIfFailed(pFactory->CreateSwapChain(mDevice.Get(), &desc, &mSwapChain));
	}

//	// rasterizer states
//
//	// wireframe
//	{
//		D3D11_RASTERIZER_DESC desc;
//		desc.FillMode = D3D11_FILL_WIREFRAME;
//		desc.CullMode = D3D11_CULL_NONE;
//		desc.FrontCounterClockwise = false;
//		desc.DepthBias = 0;
//		desc.DepthBiasClamp = 0;
//		desc.SlopeScaledDepthBias = 0;
//		desc.DepthClipEnable = true;
//		desc.ScissorEnable = false;
//		desc.MultisampleEnable = false;
//		desc.AntialiasedLineEnable = false;
//
//		HR(mDevice->CreateRasterizerState(&desc, &mWireframeRS));
//	}
//
//	// no cull
//	{
//		D3D11_RASTERIZER_DESC desc;
//		desc.FillMode = D3D11_FILL_SOLID;
//		desc.CullMode = D3D11_CULL_NONE;
//		desc.FrontCounterClockwise = false;
//		desc.DepthBias = 0;
//		desc.DepthBiasClamp = 0;
//		desc.SlopeScaledDepthBias = 0;
//		desc.DepthClipEnable = true;
//		desc.ScissorEnable = false;
//		desc.MultisampleEnable = false;
//		desc.AntialiasedLineEnable = false;
//
//		HR(mDevice->CreateRasterizerState(&desc, &mNoCullRS));
//	}
//
//	// depth stencil states
//
//	// less equal
//	{
//		D3D11_DEPTH_STENCIL_DESC desc;
//		desc.DepthEnable = true;
//		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
//		desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
//		desc.StencilEnable = false;
//		//desc.StencilReadMask = 0xff;
//		//desc.StencilWriteMask = 0xff;
//		//desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
//		//desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
//		//desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
//		//desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
//		//desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
//		//desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
//		//desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
//		//desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
//
//		HR(mDevice->CreateDepthStencilState(&desc, &mLessEqualDSS));
//	}
//
//	// equal
//	{
//		D3D11_DEPTH_STENCIL_DESC desc;
//		desc.DepthEnable = true;
//		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
//		desc.DepthFunc = D3D11_COMPARISON_EQUAL;
//		desc.StencilEnable = false;
//		//desc.StencilReadMask = 0xff;
//		//desc.StencilWriteMask = 0xff;
//		//desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
//		//desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
//		//desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
//		//desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
//		//desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
//		//desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
//		//desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
//		//desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
//
//		HR(mDevice->CreateDepthStencilState(&desc, &mEqualDSS));
//	}

	return true;
}

// void AppBase::CreateRTVAndDSVDescriptorHeaps()
// {
// 	// ImGUI SRV
// 	{
// 		D3D12_DESCRIPTOR_HEAP_DESC desc;
// 		desc.NumDescriptors = 1;
// 		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
// 		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
// 		desc.NodeMask = 0;

// 		ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mSRVHeap.GetAddressOf())));
// 	}
// }

void AppBase::NameResource(ID3D11DeviceChild* pDeviceChild, const std::string& name)
{
	ThrowIfFailed(pDeviceChild->SetPrivateData(WKPDID_D3DDebugObjectName, UINT(name.length()), name.data()));
}

void AppBase::OnResize()
{
	static UINT prevWidth = 0;
	static UINT prevHeight = 0;

	// ImGui_ImplDX12_InvalidateDeviceObjects();

	// ImGui_ImplDX12_CreateDeviceObjects();

	if ((mWindowWidth == 0) || (mWindowHeight == 0) || ((mWindowWidth == prevWidth) && (mWindowHeight == prevHeight)))
	{
		// resize only if the new size is valid and different
		return;
	}

	prevWidth = mWindowWidth;
	prevHeight = mWindowHeight;

	// back buffer
	{
		mBackBufferRTV.Reset();

		ThrowIfFailed(mSwapChain->ResizeBuffers(min(mSwapChainBufferCount, DXGI_MAX_SWAP_CHAIN_BUFFERS),
												mWindowWidth,
												mWindowHeight,
												mSwapChainFormat,
												mSwapChainFlags));

		ComPtr<ID3D11Resource> pBackBuffer;
		ThrowIfFailed(mSwapChain->GetBuffer(0,
											__uuidof(ID3D11Resource),
											reinterpret_cast<void**>(pBackBuffer.GetAddressOf())));

		NameResource(pBackBuffer.Get(), "BackBuffer");

		ThrowIfFailed(mDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &mBackBufferRTV));

		NameResource(mBackBufferRTV.Get(), "BackBufferRTV");
	}

	// depth stencil buffer
	{
		D3D11_TEXTURE2D_DESC desc;
		desc.Width = mWindowWidth;
		desc.Height = mWindowHeight;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		ComPtr<ID3D11Texture2D> pDepthStencilBuffer;
		ThrowIfFailed(mDevice->CreateTexture2D(&desc,
											   nullptr,
											   &pDepthStencilBuffer));

		NameResource(pDepthStencilBuffer.Get(), "DepthStencilBuffer");

		// depth stencil view
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC desc;
			desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			desc.Flags = 0;
			desc.Texture2D.MipSlice = 0;

			mDepthStencilBufferDSV.Reset();
			ThrowIfFailed(mDevice->CreateDepthStencilView(pDepthStencilBuffer.Get(),
														  &desc,
														  &mDepthStencilBufferDSV));

			NameResource(mDepthStencilBufferDSV.Get(), "DepthStencilBufferDSV");
		}
	}

	// viewport
	{
		mViewport.TopLeftX = 0;
		mViewport.TopLeftY = 0;
		mViewport.Width = FLOAT(mWindowWidth);
		mViewport.Height = FLOAT(mWindowHeight);
		mViewport.MinDepth = 0;
		mViewport.MaxDepth = 1;
	}

	mWindowAspectRatio = float(mWindowWidth) / float(mWindowHeight);

	mCamera.SetLens(0.25f * XM_PI, mWindowAspectRatio, 1.0f, 1000.0f);
}

// extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT AppBase::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// if (ImGui_ImplWin32_WndProcHandler(mWindow, msg, wParam, lParam))
	// {
	// 	return true;
	// }

	switch (msg)
	{
	// WM_ACTIVATE is sent when the window is activated or deactivated
	// we pause the game when the window is deactivated and unpause it when it becomes active
	case WM_ACTIVATE:
	{
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			mIsAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			mIsAppPaused = false;
			mTimer.Start();
		}
		return 0;
	}
	// WM_SIZE is sent when the user resizes the window
	case WM_SIZE:
	{
		// new client area dimensions
		mWindowWidth = LOWORD(lParam);
		mWindowHeight = HIWORD(lParam);

		if (mDevice)
		{
			switch (wParam)
			{

			case SIZE_MINIMIZED:
			{
				mIsAppPaused = true;
				mIsWindowMinimized = true;
				mIsWindowMaximized = false;
			}
			case SIZE_MAXIMIZED:
			{
				mIsAppPaused = false;
				mIsWindowMinimized = false;
				mIsWindowMaximized = true;
				OnResize();
			}
			case SIZE_RESTORED:
			{
				// restoring from minimized state?
				if (mIsWindowMinimized)
				{
					mIsAppPaused = false;
					mIsWindowMinimized = false;
					OnResize();
				}
				// restoring from maximized state?
				else if (mIsWindowMaximized)
				{
					mIsAppPaused = false;
					mIsWindowMaximized = false;
					OnResize();
				}
				else if (mIsWindowResizing)
				{
					// If user is dragging the resize bars, we do not resize
					// the buffers here because as the user continuously
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from dragging
					// the resize bars. So instead, we reset after the user is
					// done resizing the window and releases the resize bars, which
					// sends a WM_EXITSIZEMOVE message.
				}
				else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				{
					OnResize();
				}
			}
			}
		}
		return 0;
	}
	// WM_EXITSIZEMOVE is sent when the user grabs the resize bars
	case WM_ENTERSIZEMOVE:
	{
		mIsAppPaused = true;
		mIsWindowResizing = true;
		mTimer.Stop();
		return 0;
	}
	// WM_EXITSIZEMOVE is sent when the user releases the resize bars
	case WM_EXITSIZEMOVE:
	{
		mIsAppPaused = false;
		mIsWindowResizing = false;
		mTimer.Start();
		OnResize(); // we reset everything based on the new window dimensions
		return 0;
	}
	// WM_DESTROY is sent when the window is being destroyed
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		return 0;
	}
	// WM_MENUCHAR is sent when a menu is active and the user presses a key that does not correspond to any mnemonic or accelerator key
	case WM_MENUCHAR:
	{
		// don't beep when we alt-enter
		return MAKELRESULT(0, MNC_CLOSE);
	}
	// catch this message so to prevent the window from becoming too small
	case WM_GETMINMAXINFO:
	{
		((MINMAXINFO *)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO *)lParam)->ptMinTrackSize.y = 200;
		return 0;
	}
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
	{
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	}
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	{
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	}
	case WM_MOUSEMOVE:
	{
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	}
	case WM_KEYUP:
	{
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		return 0;
	}
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void AppBase::OnMouseDown(WPARAM state, int x, int y)
{
	mLastMousePosition.x = x;
	mLastMousePosition.y = y;

	SetCapture(mWindow);
}

void AppBase::OnMouseUp(WPARAM state, int x, int y)
{
	ReleaseCapture();
}

void AppBase::OnMouseMove(WPARAM state, int x, int y)
{
	auto clamp = [](const float& x, const float& a, const float& b) -> float
	{
		return x < a ? a : (x > b ? b : x);
	};

	if ((state & MK_LBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePosition.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePosition.y));

		mCamera.Pitch(dy);
		mCamera.RotateY(dx);
	}

	mLastMousePosition.x = x;
	mLastMousePosition.y = y;
}

void AppBase::OnKeyboardEvent(const Timer& timer)
{
	const float dt = timer.GetDeltaTime();

	if ((GetAsyncKeyState('W') & 0x8000))
	{
		mCamera.Walk(+10.0f * dt);
	}
	if ((GetAsyncKeyState('S') & 0x8000))
	{
		mCamera.Walk(-10.0f * dt);
	}
	if ((GetAsyncKeyState('A') & 0x8000))
	{
		mCamera.Strafe(-10.0f * dt);
	}
	if ((GetAsyncKeyState('D') & 0x8000))
	{
		mCamera.Strafe(+10.0f * dt);
	}
}

int AppBase::Run()
{
	MSG msg = {0};

	mTimer.Reset();

	// IMGUI_CHECKVERSION();
	// ImGui::CreateContext();
	// ImGuiIO &io = ImGui::GetIO();
	// (void)io;
	// // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	// // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// ImGui::StyleColorsDark();
	// // ImGui::StyleColorsClassic();

	// ImGui_ImplWin32_Init(mWindow);
	// ImGui_ImplDX12_Init(mDevice.Get(),
	// 					SwapChainBufferSize,
	// 					mBackBufferFormat,
	// 					mSRVHeap.Get(),
	// 					mSRVHeap->GetCPUDescriptorHandleForHeapStart(),
	// 					mSRVHeap->GetGPUDescriptorHandleForHeapStart());

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			mTimer.Tick();

			if (!mIsAppPaused)
			{
				//CalculateFrameStats();
				Update(mTimer);
				Draw(mTimer);

				ThrowIfFailed(mSwapChain->Present(0, 0));
			}
			else
			{
				Sleep(100);
			}
		}
	}

	// ImGui_ImplDX12_Shutdown();
	// ImGui_ImplWin32_Shutdown();
	// ImGui::DestroyContext();

	return int(msg.wParam);
}

// void AppBase::CalculateFrameStats()
// {
// 	static int FrameCount = 0;
// 	static float TimeElapsed = 0.0f;

// 	FrameCount++;

// 	// compute averages over one second period
// 	if ((mTimer.GetTotalTime() - TimeElapsed) >= 1.0f)
// 	{
// 		const float fps = static_cast<float>(FrameCount); // fps = FrameCount / 1 sec
// 		const float mspf = 1000.0f / fps;				  // milliseconds per frame

// 		const std::wstring windowText = mWindowTitle +
// 										L"    fps: " + std::to_wstring(fps) +
// 										L"   mspf: " + std::to_wstring(mspf);

// 		SetWindowText(mWindow, windowText.c_str());

// 		// reset for next average
// 		FrameCount = 0;
// 		TimeElapsed += 1.0f;
// 	}
// }

void AppBase::UpdateMainPassCB(const Timer& timer)
{
	MainPassCB buffer;

	XMMATRIX V = mCamera.GetView();
	XMMATRIX P = mCamera.GetProj();
	XMMATRIX viewProj = V * P;
	XMStoreFloat4x4(&buffer.viewProj, viewProj);

	buffer.eyePosition = mCamera.GetPositionF();

	buffer.ambientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

	buffer.lights[0].direction = mLighting.GetLightDirection(0);
	buffer.lights[0].strength = { 0.9f, 0.9f, 0.7f };
	buffer.lights[1].direction = mLighting.GetLightDirection(1);
	buffer.lights[1].strength = { 0.4f, 0.4f, 0.4f };;
	buffer.lights[2].direction = mLighting.GetLightDirection(2);
	buffer.lights[2].strength = { 0.2f, 0.2f, 0.2f };
	
	mContext->UpdateSubresource(mMainPassCB.Get(), 0, nullptr, &buffer, 0, 0);
}

void AppBase::UpdateMaterialsSB(const Timer& timer)
{
	if (mIsMaterialsBufferDirty)
	{
		mIsMaterialsBufferDirty = false;
	}
}

void AppBase::Update(const Timer& timer)
{
	OnKeyboardEvent(timer);

	mCamera.UpdateViewMatrix();

	mLighting.UpdateLights(timer);

	UpdateMainPassCB(timer);
	UpdateMaterialsSB(timer);
}

ComPtr<ID3DBlob> AppBase::CompileShader(const std::wstring& fileName,
										const D3D_SHADER_MACRO* defines,
										const std::string& entryPoint,
										const ShaderTarget target)
{
	UINT flags = 0;
#ifdef _DEBUG
	flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif // _DEBUG

	HRESULT result = S_OK;

	ComPtr<ID3DBlob> code = nullptr;
	ComPtr<ID3DBlob> errors = nullptr;

	std::string targetName;

	switch (target)
	{
		case ShaderTarget::VS:
			targetName = "vs_5_0";
			break;
		case ShaderTarget::PS:
			targetName = "ps_5_0";
			break;
	}

	result = D3DCompileFromFile(fileName.c_str(),
								defines,
								D3D_COMPILE_STANDARD_FILE_INCLUDE,
								entryPoint.c_str(),
								targetName.c_str(),
								flags,
								0,
								&code,
								&errors);

#ifdef _DEBUG
	if (errors != nullptr)
	{
		OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
	}
#endif // _DEBUG

	ThrowIfFailed(result);

	return code;
}
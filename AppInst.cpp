#include "AppInst.h"

// d3d
#include <directxcolors.h>

AppInst::AppInst(HINSTANCE instance)
	: AppBase(instance)
{}

AppInst::~AppInst() {}

bool AppInst::Init()
{
	if (!AppBase::Init())
	{
		return false;
	}

	mCamera.SetPosition(0.0f, 0.0f, -5.0f);

	ComPtr<ID3D11DepthStencilState> pReflectiveSurfaceDSS;
	ComPtr<ID3D11DepthStencilState> pNonReflectiveSurfaceDSS;

	// depth stencil state
	{
		D3D11_DEPTH_STENCIL_DESC desc;
		desc.DepthEnable = TRUE;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D11_COMPARISON_LESS;
		desc.StencilEnable = TRUE;
		desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
		desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
		desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
		desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		desc.BackFace = desc.FrontFace;

		ThrowIfFailed(mDevice->CreateDepthStencilState(&desc, &pReflectiveSurfaceDSS));
		NameResource(pReflectiveSurfaceDSS.Get(), "ReflectiveSurfaceDSS");

		desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR_SAT;
		desc.BackFace = desc.FrontFace;

		ThrowIfFailed(mDevice->CreateDepthStencilState(&desc, &pNonReflectiveSurfaceDSS));
		NameResource(pNonReflectiveSurfaceDSS.Get(), "NonReflectiveSurfaceDSS");
	}

	// bridge
	{
		Object object;

		MeshData mesh = MeshManager::LoadModel("models/bridge_ib_order.csv");
		//MeshData mesh = MeshManager::LoadModel("models/bridge_vb_order.csv");
		object.mesh = mMeshManager.AddMesh("bridge", mesh);

		Material material;
		XMStoreFloat4(&material.diffuse, DirectX::Colors::White);
		material.fresnel = XMFLOAT3(0.1f, 0.1f, 0.1f);
		material.roughness = 0.3f;
		material.diffuseTextureIndex = int(mTextureManager.LoadTexture("textures/bridge_diff.dds"));
		material.normalTextureIndex = int(mTextureManager.LoadTexture("textures/bridge_norm.dds"));
		object.material = mMaterialManager.AddMaterial("bridge", material);
		
		object.depthStencilState = pNonReflectiveSurfaceDSS;

		// default unpack normal pixel shader
		{
			std::wstring path = L"../RenderToyD3D11/shaders/Default.hlsl";

			const D3D_SHADER_MACRO defines[] =
			{
				"SHADOW_MAPPING", "1",
				"UNPACK_NORMAL", "1",
				nullptr, nullptr
			};

			ComPtr<ID3DBlob> pCode = CompileShader(path,
												   defines,
												   "DefaultPS",
												   ShaderTarget::PS);

			ThrowIfFailed(mDevice->CreatePixelShader(pCode->GetBufferPointer(),
													 pCode->GetBufferSize(),
													 nullptr,
													 &object.pixelShader));

			NameResource(object.pixelShader.Get(), "DefaultUnpackNormalPS");
		}

		mObjectManager.AddObject(object);
	}

	// reflective grid
	{
		Object object;

		MeshData mesh = MeshManager::CreateGrid(50, 50, 2, 2);
		object.mesh = mMeshManager.AddMesh("grid", mesh);

		XMStoreFloat4x4(&object.world, XMMatrixTranslation(1, -3, 1));

		Material material;
		XMStoreFloat4(&material.diffuse, DirectX::Colors::DarkSlateGray);
		//material.fresnel = XMFLOAT3(0.98f, 0.97f, 0.95f);
		material.fresnel = XMFLOAT3(0.5f, 0.5f, 0.5f);
		material.roughness = 0.1f;
		object.material = mMaterialManager.AddMaterial("grid", material);
		
		object.depthStencilState = pReflectiveSurfaceDSS;

		// default reflective surface vertex shader
		{
			std::wstring path = L"../RenderToyD3D11/shaders/Default.hlsl";

			const D3D_SHADER_MACRO defines[] =
			{
				//"SHADOW_MAPPING", "1",
				//"REFLECTIVE_SURFACE", "1",
				"WATER_NORMAL_MAPPING", "1",
				nullptr, nullptr
			};

			ComPtr<ID3DBlob> pCode = CompileShader(path,
												   defines,
												   "DefaultVS",
												   ShaderTarget::VS);

			ThrowIfFailed(mDevice->CreateVertexShader(pCode->GetBufferPointer(),
													  pCode->GetBufferSize(),
													  nullptr,
													  &object.vertexShader));

			NameResource(object.vertexShader.Get(), "DefaultReflectiveSurfaceVS");
		}

		// default reflective surface pixel shader
		{
			std::wstring path = L"../RenderToyD3D11/shaders/Default.hlsl";

			const D3D_SHADER_MACRO defines[] =
			{
				"SHADOW_MAPPING", "1",
				"REFLECTIVE_SURFACE", "1",
				"WATER_NORMAL_MAPPING", "1",
				nullptr, nullptr
			};

			ComPtr<ID3DBlob> pCode = CompileShader(path,
												   defines,
												   "DefaultPS",
												   ShaderTarget::PS);

			ThrowIfFailed(mDevice->CreatePixelShader(pCode->GetBufferPointer(),
													 pCode->GetBufferSize(),
													 nullptr,
													 &object.pixelShader));

			NameResource(object.pixelShader.Get(), "DefaultReflectiveSurfacePS");
		}

		mObjectManager.AddObject(object);
	}

	//const std::vector<std::string> paths =
	//{
	//	"textures/WoodCrate01.dds",
	//	"textures/WoodCrate02.dds",
	//};

	//mTextureManager.LoadTexturesIntoTexture2DArray("DiffuseTextureArray", paths);

	mBVH.Init(mDevice, mContext);
	mBVH.BuildBVH(mObjectManager.GetObjects(), mMeshManager, mMaterialManager);

	mRayTraced.Init(mDevice, mContext);

	mWavesNormalMapOffset0 = XMFLOAT2(0, 0);
	mWavesNormalMapOffset1 = XMFLOAT2(0, 0);

	// waves constant buffer
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(WavesCB);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		ThrowIfFailed(mDevice->CreateBuffer(&desc, nullptr, &mWavesCB));

		NameResource(mWavesCB.Get(), "WavesCB");
	}

	mTextureManager.LoadTexture("textures/waves0.dds");
	mTextureManager.LoadTexture("textures/waves1.dds");

	return true;
}

void AppInst::Update(const Timer& timer)
{
	AppBase::Update(timer);

	mRayTraced.UpdateCBs(mCamera.GetViewProjInvF(),
						 mLighting.GetLightDirection(0),
						 mCamera.GetPositionF());

	// update waves constant buffer
	{
		WavesCB buffer;

		const float dt = timer.GetDeltaTime();

		// normal map 0
		{
			mWavesNormalMapOffset0.x += 0.05f * dt;
			mWavesNormalMapOffset0.y += 0.20f * dt;

			XMMATRIX S = XMMatrixScaling(22.0f, 22.0f, 1.0f);
			XMMATRIX T = XMMatrixTranslation(mWavesNormalMapOffset0.x, mWavesNormalMapOffset0.y, 0.0f);
			XMStoreFloat4x4(&buffer.wavesNormalMapTransform0, S * T);
		}

		// normal map 1
		{
			mWavesNormalMapOffset1.x -= 0.02f * dt;
			mWavesNormalMapOffset1.y += 0.05f * dt;

			XMMATRIX S = XMMatrixScaling(16.0f, 16.0f, 1.0f);
			XMMATRIX T = XMMatrixTranslation(mWavesNormalMapOffset1.x, mWavesNormalMapOffset1.y, 0.0f);
			XMStoreFloat4x4(&buffer.wavesNormalMapTransform1, S * T);
		}

		mContext->UpdateSubresource(mWavesCB.Get(), 0, nullptr, &buffer, 0, 0);
	}
}

void AppInst::Draw(const Timer& timer)
{
	//enum class SRVSlotIndex
	//{
	//	material,
	//	diffuse,
	//	shadows_resolve,
	//};

	// globals
	{
		// set sampler states
		ID3D11SamplerState* samplers[]
		{
			nullptr,
			nullptr,
			mSamplerLinearWrap.Get(),
		};
		mContext->PSSetSamplers(0, sizeof(samplers) / sizeof(samplers[0]), samplers);

		// set shader resource views
		ID3D11ShaderResourceView* SRVs[] =
		{
			mMaterialManager.GetBufferSRV(),
			mTextureManager.GetSRV(0), // diffuse
			mTextureManager.GetSRV(1), // normal
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			mTextureManager.GetSRV(2), // waves normal 0
			mTextureManager.GetSRV(3), // waves normal 1
		};
		mContext->PSSetShaderResources(0, sizeof(SRVs) / sizeof(SRVs[0]), SRVs);

		// set main pass constant buffer
		mContext->VSSetConstantBuffers(0, 1, mMainPassCB.GetAddressOf());
		mContext->PSSetConstantBuffers(0, 1, mMainPassCB.GetAddressOf());

		// set waves constant buffer
		mContext->VSSetConstantBuffers(2, 1, mWavesCB.GetAddressOf());
	}

	// depth/gbuffer prepass
	{
		mUserDefinedAnnotation->BeginEvent(L"depth/gbuffer prepass");
#if IMGUI
		GPUProfilerTimestamp(TimestampQueryType::DepthGbufferPrepassBegin);
#endif // IMGUI

		// clear depth stencil buffer
		mContext->ClearDepthStencilView(mDepthStencilBufferDSV.Get(),
										D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
										1.0f,
										1); // stencil == 0 means reflective surface

		// clear gbuffer
		const FLOAT clearGBuffer[] = { 0, 0, 0, -1 };
		mContext->ClearRenderTargetView(mGBufferRTV.Get(), clearGBuffer);

		// set back buffer, gbuffer and depth stencil buffer
		mContext->OMSetRenderTargets(1,
									 mGBufferRTV.GetAddressOf(),
									 mDepthStencilBufferDSV.Get());

		// set viewport
		mContext->RSSetViewports(1, &mViewport);

		// set input layout
		mContext->IASetInputLayout(mInputLayout.Get());

		// set primitive topology
		mContext->IASetPrimitiveTopology(mPrimitiveTopology);

		// set vertex buffer
		const UINT stride = sizeof(VertexData);
		const UINT offset = 0;
		mContext->IASetVertexBuffers(0, 1, mMeshManager.GetAddressOfVertexBuffer(), &stride, &offset);

		// set index buffer
		mContext->IASetIndexBuffer(mMeshManager.GetIndexBuffer(), mMeshManager.GetIndexBufferFormat(), 0);

		// set object constant buffer
		mContext->VSSetConstantBuffers(1, 1, mObjectManager.GetAddressOfBuffer());
		mContext->PSSetConstantBuffers(1, 1, mObjectManager.GetAddressOfBuffer());

		// set vertex shader
		mContext->VSSetShader(mDefaultVS.Get(), nullptr, 0);

		// set pixel shader
		//mContext->PSSetShader(mGBufferPS.Get(), nullptr, 0);
		mContext->PSSetShader(mGBufferUnpackNormalPS.Get(), nullptr, 0);

		for (std::size_t i = 0; i < mObjectManager.GetObjects().size(); ++i)
		{
			// update object constant buffer with object data
			mObjectManager.UpdateBuffer(i);

			const Object& object = mObjectManager.GetObject(i);
			const MeshData& mesh = mMeshManager.GetMesh(object.mesh);

			if (object.depthStencilState.Get())
			{
				mContext->OMSetDepthStencilState(object.depthStencilState.Get(), object.stencilRef);
			}

			// draw
			if (mesh.indexCount == 0)
			{
				mContext->Draw(UINT(mesh.vertices.size()), mesh.vertexBase);
			}
			else
			{
				mContext->DrawIndexed(mesh.indexCount, mesh.indexStart, mesh.vertexBase);
			}

			if (object.depthStencilState.Get())
			{
				mContext->OMSetDepthStencilState(nullptr, 0);
			}
		}

		// clear depth stencil state
		mContext->OMSetDepthStencilState(nullptr, 0);

#if IMGUI
		GPUProfilerTimestamp(TimestampQueryType::DepthGbufferPrepassEnd);
#endif // IMGUI
		mUserDefinedAnnotation->EndEvent();
	}

	// shadows map
	// (shadows resolve)
	
	// raytraced shadows
	{
		mUserDefinedAnnotation->BeginEvent(L"raytraced shadows");
#if IMGUI
		GPUProfilerTimestamp(TimestampQueryType::RayTracedShadowsBegin);
#endif // IMGUI

		// clear shadow render target
		mContext->ClearRenderTargetView(mShadowsResolveRTV.Get(), DirectX::Colors::White);

		// set shadow render target
		mContext->OMSetRenderTargets(1,
									 mShadowsResolveRTV.GetAddressOf(),
									 nullptr);

		// set input layout
		mContext->IASetInputLayout(nullptr);

		// set vertex shader
		mContext->VSSetShader(mFullscreenVS.Get(), nullptr, 0);

		// set shader resource views
		ID3D11ShaderResourceView* SRVs[] =
		{
			mDepthBufferSRV.Get(),
			mGBufferSRV.Get(),
			mBVH.GetTreeBufferSRV(),
		};
		mContext->PSSetShaderResources(5, sizeof(SRVs) / sizeof(SRVs[0]), SRVs);

		// set pixel shader
		mContext->PSSetShader(mRayTraced.GetShadowsPS(), nullptr, 0);

		// set constant buffer
		ID3D11Buffer* buffer = mRayTraced.GetShadowsCB();
		mContext->PSSetConstantBuffers(1, 1, &buffer);

		// draw
		mContext->Draw(3, 0);

		// set shader resource views
		ID3D11ShaderResourceView* pNullSRVs[] =
		{
			nullptr,
			nullptr,
			nullptr,
		};
		mContext->PSSetShaderResources(5, sizeof(pNullSRVs) / sizeof(pNullSRVs[0]), pNullSRVs);

#if IMGUI
		GPUProfilerTimestamp(TimestampQueryType::RayTracedShadowsEnd);
#endif // IMGUI
		mUserDefinedAnnotation->EndEvent();
	}
	
	// SSPR
	{
		// unimplemented
	}
	
	// raytraced reflections
	{
		mUserDefinedAnnotation->BeginEvent(L"raytraced reflections");
#if IMGUI
		GPUProfilerTimestamp(TimestampQueryType::RayTracedReflectionsBegin);
#endif // IMGUI

		// clear reflections render target
		mContext->ClearRenderTargetView(mReflectionsResolveRTV.Get(), DirectX::Colors::Transparent);

		// set reflections render target
		mContext->OMSetRenderTargets(1,
									 mReflectionsResolveRTV.GetAddressOf(),
									 mDepthStencilBufferReadOnlyDSV.Get());

		// set input layout
		mContext->IASetInputLayout(nullptr);

		// set vertex shader
		mContext->VSSetShader(mFullscreenVS.Get(), nullptr, 0);

		// set shader resource views
		ID3D11ShaderResourceView* pSRVs[] =
		{
			mShadowsResolveSRV.Get(),
			nullptr,
			mDepthBufferSRV.Get(),
			mGBufferSRV.Get(),
			mBVH.GetTreeBufferSRV(),
			mBVH.GetVertexBufferSRV(),
		};
		mContext->PSSetShaderResources(3, sizeof(pSRVs) / sizeof(pSRVs[0]), pSRVs);

		// set pixel shader
		//mContext->PSSetShader(mRayTraced.GetReflectionsPS(), nullptr, 0);
		mContext->PSSetShader(mRayTraced.GetReflectionsUnpackNormalPS(), nullptr, 0);

		// set depth stencil state
		mContext->OMSetDepthStencilState(mRayTraced.GetReflectionsDSS(), 0);

		// draw
		mContext->Draw(3, 0);

		mContext->OMSetDepthStencilState(nullptr, 0);

		// set shader resource views
		ID3D11ShaderResourceView* pNullSRVs[] =
		{
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
		};
		mContext->PSSetShaderResources(3, sizeof(pNullSRVs) / sizeof(pNullSRVs[0]), pNullSRVs);

#if IMGUI
		GPUProfilerTimestamp(TimestampQueryType::RayTracedReflectionsEnd);
#endif // IMGUI
		mUserDefinedAnnotation->EndEvent();
	}

	// main pass
	{
		mUserDefinedAnnotation->BeginEvent(L"main pass");
#if IMGUI
		GPUProfilerTimestamp(TimestampQueryType::MainPassBegin);
#endif // IMGUI

		// clear back buffer
		mContext->ClearRenderTargetView(mBackBufferRTV.Get(), DirectX::Colors::Magenta);

		// set back buffer, gbuffer and depth stencil buffer
		mContext->OMSetRenderTargets(1,
									 mBackBufferRTV.GetAddressOf(),
									 mDepthStencilBufferReadOnlyDSV.Get());

		// set viewport
		mContext->RSSetViewports(1, &mViewport);

		// set input layout
		mContext->IASetInputLayout(mInputLayout.Get());

		// set primitive topology
		mContext->IASetPrimitiveTopology(mPrimitiveTopology);

		// set vertex buffer
		const UINT stride = sizeof(VertexData);
		const UINT offset = 0;
		mContext->IASetVertexBuffers(0, 1, mMeshManager.GetAddressOfVertexBuffer(), &stride, &offset);

		// set index buffer
		mContext->IASetIndexBuffer(mMeshManager.GetIndexBuffer(), mMeshManager.GetIndexBufferFormat(), 0);

		// set object constant buffer
		mContext->VSSetConstantBuffers(1, 1, mObjectManager.GetAddressOfBuffer());
		mContext->PSSetConstantBuffers(1, 1, mObjectManager.GetAddressOfBuffer());

		// set vertex shader
		mContext->VSSetShader(mDefaultVS.Get(), nullptr, 0);

		// set pixel shader
		mContext->PSSetShader(mDefaultPS.Get(), nullptr, 0);

		// set depth stencil state
		mContext->OMSetDepthStencilState(mDepthEqualDSS.Get(), 0);

		ID3D11ShaderResourceView* pSRVs[] =
		{
			mShadowsResolveSRV.Get(),
			mReflectionsResolveSRV.Get(),
		};
		mContext->PSSetShaderResources(3, sizeof(pSRVs) / sizeof(pSRVs[0]), pSRVs);

		for (std::size_t i = 0; i < mObjectManager.GetObjects().size(); ++i)
		{
			// update object constant buffer with object data
			mObjectManager.UpdateBuffer(i);

			const Object& object = mObjectManager.GetObject(i);
			const MeshData& mesh = mMeshManager.GetMesh(object.mesh);

			if (object.vertexShader.Get())
			{
				mContext->VSSetShader(object.vertexShader.Get(), nullptr, 0);
			}

			if (object.pixelShader.Get())
			{
				mContext->PSSetShader(object.pixelShader.Get(), nullptr, 0);
			}

			// draw
			if (mesh.indexCount == 0)
			{
				mContext->Draw(UINT(mesh.vertices.size()), mesh.vertexBase);
			}
			else
			{
				mContext->DrawIndexed(mesh.indexCount, mesh.indexStart, mesh.vertexBase);
			}

			if (object.vertexShader.Get())
			{
				mContext->VSSetShader(mDefaultVS.Get(), nullptr, 0);
			}

			if (object.pixelShader.Get())
			{
				mContext->PSSetShader(mDefaultPS.Get(), nullptr, 0);
			}
		}

		ID3D11ShaderResourceView* pNullSRVs[] =
		{
			nullptr,
			nullptr,
		};
		mContext->PSSetShaderResources(3, sizeof(pNullSRVs) / sizeof(pNullSRVs[0]), pNullSRVs);

		mContext->OMSetDepthStencilState(nullptr, 0);

#if IMGUI
		GPUProfilerTimestamp(TimestampQueryType::MainPassEnd);
#endif // IMGUI
		mUserDefinedAnnotation->EndEvent();
	}
}

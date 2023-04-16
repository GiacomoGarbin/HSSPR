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


	MeshData mesh = MeshManager::CreateBox(1, 1, 1);

	Material material;
	material.diffuse = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	//XMStoreFloat4(&material.diffuse, DirectX::Colors::Cyan);
	material.fresnel = XMFLOAT3(0.6f, 0.6f, 0.6f);
	material.roughness = 0.2f;
	material.diffuseTextureIndex = 0; // mTextureManager.LoadTexture("textures/WoodCrate01.dds");

	Object object;
	object.mesh = mMeshManager.AddMesh("box", mesh);
	object.material = mMaterialManager.AddMaterial("default", material);
	XMStoreFloat4x4(&object.world, XMMatrixScaling(10, 1, 10) * XMMatrixTranslation(0, -1, 0));

	// pixel shader
	{
		std::wstring path = L"../RenderToyD3D11/shaders/Default.hlsl";

		const D3D_SHADER_MACRO defines[] =
		{
			"REFLECTIVE_SURFACE", "1",
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

		ThrowIfFailed(mDevice->CreateDepthStencilState(&desc, &object.depthStencilState));
		NameResource(object.depthStencilState.Get(), "ReflectiveSurfaceDSS");

		mObjectManager.AddObject(object);

		desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR_SAT;
		desc.BackFace = desc.FrontFace;

		object.depthStencilState.Reset();
		ThrowIfFailed(mDevice->CreateDepthStencilState(&desc, &object.depthStencilState));
		NameResource(object.depthStencilState.Get(), "NonReflectiveSurfaceDSS");
	}

	XMVECTORF32 colors[] =
	{
		DirectX::Colors::Crimson,
		DirectX::Colors::Cyan,
		DirectX::Colors::Yellow,
		DirectX::Colors::Green,
		DirectX::Colors::Red,
		DirectX::Colors::Orange,
		DirectX::Colors::Blue,
		DirectX::Colors::Violet,
		DirectX::Colors::Brown,
	};

	float radius = 1;

	//material.roughness = 1;
	material.diffuseTextureIndex = 0;

	object.pixelShader.Reset();

	for (float x = -radius; x <= +radius; ++x)
	{
		for (float z = -radius; z <= +radius; ++z)
		{
			//if (x != 0 || z != 0) continue;

			int i = (x + radius) * (radius*2+1) + (z + radius);

			XMStoreFloat4x4(&object.world, XMMatrixTranslation(x * 2, 1, z * 2));

			XMStoreFloat4(&material.diffuse, colors[i]);
			object.material = mMaterialManager.AddMaterial("box" + std::to_string(i), material);

			mObjectManager.AddObject(object);
		}
	}

	const std::vector<std::string> paths =
	{
		"textures/WoodCrate01.dds",
		"textures/WoodCrate02.dds",
	};

	mTextureManager.LoadTexturesIntoTexture2DArray("DiffuseTextureArray", paths);

	mBVH.Init(mDevice, mContext);
	mBVH.BuildBVH(mObjectManager.GetObjects(), mMeshManager, mMaterialManager);

	mRayTraced.Init(mDevice, mContext);

	return true;
}

void AppInst::Update(const Timer& timer)
{
	AppBase::Update(timer);

	mRayTraced.UpdateCBs(mCamera.GetViewProjInvF(),
						 mLighting.GetLightDirection(0),
						 mCamera.GetPositionF());
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
			mTextureManager.GetSRV(0), // diffuse + normal, or only diffuse?
		};
		mContext->PSSetShaderResources(0, sizeof(SRVs) / sizeof(SRVs[0]), SRVs);
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

		// set main pass constant buffer
		mContext->VSSetConstantBuffers(0, 1, mMainPassCB.GetAddressOf());
		mContext->PSSetConstantBuffers(0, 1, mMainPassCB.GetAddressOf());

		// set object constant buffer
		mContext->VSSetConstantBuffers(1, 1, mObjectManager.GetAddressOfBuffer());
		mContext->PSSetConstantBuffers(1, 1, mObjectManager.GetAddressOfBuffer());

		// set vertex shader
		mContext->VSSetShader(mDefaultVS.Get(), nullptr, 0);

		// set pixel shader
		mContext->PSSetShader(mGBufferPS.Get(), nullptr, 0);

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
			mContext->DrawIndexed(mesh.indexCount, mesh.indexStart, mesh.vertexBase);

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
		mContext->PSSetShaderResources(4, sizeof(SRVs) / sizeof(SRVs[0]), SRVs);

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
		mContext->PSSetShaderResources(4, sizeof(pNullSRVs) / sizeof(pNullSRVs[0]), pNullSRVs);

#if IMGUI
		GPUProfilerTimestamp(TimestampQueryType::RayTracedShadowsEnd);
#endif // IMGUI
		mUserDefinedAnnotation->EndEvent();
	}
	
	// SSPR
	
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
		mContext->PSSetShaderResources(2, sizeof(pSRVs) / sizeof(pSRVs[0]), pSRVs);

		// set pixel shader
		mContext->PSSetShader(mRayTraced.GetReflectionsPS(), nullptr, 0);

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
		mContext->PSSetShaderResources(2, sizeof(pNullSRVs) / sizeof(pNullSRVs[0]), pNullSRVs);

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

		// set main pass constant buffer
		mContext->VSSetConstantBuffers(0, 1, mMainPassCB.GetAddressOf());
		mContext->PSSetConstantBuffers(0, 1, mMainPassCB.GetAddressOf());

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
		mContext->PSSetShaderResources(2, sizeof(pSRVs) / sizeof(pSRVs[0]), pSRVs);

		for (std::size_t i = 0; i < mObjectManager.GetObjects().size(); ++i)
		{
			// update object constant buffer with object data
			mObjectManager.UpdateBuffer(i);

			const Object& object = mObjectManager.GetObject(i);
			const MeshData& mesh = mMeshManager.GetMesh(object.mesh);

			if (object.pixelShader.Get())
			{
				mContext->PSSetShader(object.pixelShader.Get(), nullptr, 0);
			}

			// draw
			mContext->DrawIndexed(mesh.indexCount, mesh.indexStart, mesh.vertexBase);

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
		mContext->PSSetShaderResources(2, sizeof(pNullSRVs) / sizeof(pNullSRVs[0]), pNullSRVs);

#if IMGUI
		GPUProfilerTimestamp(TimestampQueryType::MainPassEnd);
#endif // IMGUI
		mUserDefinedAnnotation->EndEvent();
	}

	// ================================================================================

	//// ray traced
	//{
	//	//// set back buffer 
	//	//ID3D11RenderTargetView* RTVs[] =
	//	//{
	//	//	mBackBufferRTV.Get(),
	//	//	nullptr,
	//	//};
	//	//mContext->OMSetRenderTargets(sizeof(RTVs) / sizeof(RTVs[0]),
	//	//							 RTVs,
	//	//							 mDepthStencilBufferReadOnlyDSV.Get());

	//	// set input layout
	//	mContext->IASetInputLayout(nullptr);

	//	// set vertex shader
	//	mContext->VSSetShader(mFullscreenVS.Get(), nullptr, 0);

	//	// set shader resource views
	//	ID3D11ShaderResourceView* SRVs[] =
	//	{
	//		mDepthBufferSRV.Get(),
	//		mGBufferSRV.Get(),
	//		mBVH.GetTreeBufferSRV(),
	//		mBVH.GetVertexBufferSRV(),
	//	};
	//	mContext->PSSetShaderResources(3, sizeof(SRVs) / sizeof(SRVs[0]), SRVs);

	//	// set blend state
	//	mContext->OMSetBlendState(mRayTraced.GetBlendState(), nullptr, 0xffffffff);

	//	// reflections
	//	{
	//		mUserDefinedAnnotation->BeginEvent(L"raytraced reflections");

	//		// set pixel shader
	//		mContext->PSSetShader(mRayTraced.GetReflectionsPS(), nullptr, 0);

	//		// set constant buffer
	//		ID3D11Buffer* CBs[] =
	//		{
	//			mRayTraced.GetReflectionsCB()
	//		};
	//		mContext->PSSetConstantBuffers(1, sizeof(CBs) / sizeof(CBs[0]), CBs);

	//		// set depth stencil state
	//		mContext->OMSetDepthStencilState(mRayTraced.GetReflectionsDSS(), 0);

	//		// draw
	//		mContext->Draw(3, 0);

	//		GPUProfilerTimestamp(TimestampQueryType::RayTracedReflections);

	//		mUserDefinedAnnotation->EndEvent();
	//	}

	//	// set shader resource views
	//	ID3D11ShaderResourceView* NullSRVs[] =
	//	{
	//		nullptr,
	//		nullptr,
	//		nullptr,
	//		nullptr,
	//	};
	//	mContext->PSSetShaderResources(3, sizeof(NullSRVs) / sizeof(NullSRVs[0]), NullSRVs);

	//	mContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);

	//	mContext->OMSetDepthStencilState(nullptr, 0);
	//}
}

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
	material.fresnel = XMFLOAT3(0.6f, 0.6f, 0.6f);
	material.roughness = 0.2f;
	material.diffuseTextureIndex = 0; // mTextureManager.LoadTexture("textures/WoodCrate01.dds");

	Object object;
	object.mesh = mMeshManager.AddMesh("box", mesh);
	object.material = mMaterialManager.AddMaterial("default", material);
	XMStoreFloat4x4(&object.world, XMMatrixScaling(10, 1, 10) * XMMatrixTranslation(0, -1, 0));

	mObjectManager.AddObject(object);

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
	// clear back buffer
	mContext->ClearRenderTargetView(mBackBufferRTV.Get(),
									DirectX::Colors::Magenta);

	// clear depth stencil buffer
	mContext->ClearDepthStencilView(mDepthStencilBufferDSV.Get(),
									D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 
									1.0f,
									0);

	// set back buffer and depth stencil buffer
	mContext->OMSetRenderTargets(1,
								 mBackBufferRTV.GetAddressOf(),
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

	// set rasterizer state
	// set depth stencil state
	// set blend state

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
		mTextureManager.GetSRV(0),
	};
	mContext->PSSetShaderResources(0, sizeof(SRVs) / sizeof(SRVs[0]), SRVs);

	for (std::size_t i = 0; i < mObjectManager.GetObjects().size(); ++i)
	{
		mObjectManager.UpdateBuffer(i);

		const MeshData& mesh = mMeshManager.GetMesh(mObjectManager.GetObject(i).mesh);

		// draw
		mContext->DrawIndexed(mesh.indexCount, mesh.indexStart, mesh.vertexBase);
	}

	// ray traced
	{
		// set back buffer and depth stencil buffer
		mContext->OMSetRenderTargets(1,
									 mBackBufferRTV.GetAddressOf(),
									 nullptr);

		// set input layout
		mContext->IASetInputLayout(nullptr);

		// set vertex shader
		mContext->VSSetShader(mFullscreenVS.Get(), nullptr, 0);

		// set shader resource views
		ID3D11ShaderResourceView* SRVs[] =
		{
			mDepthBufferSRV.Get(),
			mBVH.GetTreeBufferSRV(),
			mBVH.GetVertexBufferSRV(),
		};
		mContext->PSSetShaderResources(2, sizeof(SRVs) / sizeof(SRVs[0]), SRVs);

		// set blend state
		mContext->OMSetBlendState(mRayTraced.GetBlendState(), nullptr, 0xffffffff);
		
		GPUProfilerTimestamp(TimestampQueryType::RayTracedBegin);

		// shadows
		{
			// set pixel shader
			mContext->PSSetShader(mRayTraced.GetShadowsPS(), nullptr, 0);

			// set constant buffer
			ID3D11Buffer* CBs[] =
			{
				mRayTraced.GetShadowsCB()
			};
			mContext->PSSetConstantBuffers(1, sizeof(CBs) / sizeof(CBs[0]), CBs);

			// draw
			mContext->Draw(3, 0);

			GPUProfilerTimestamp(TimestampQueryType::RayTracedShadows);
		}

		// reflections
		{
			// set pixel shader
			mContext->PSSetShader(mRayTraced.GetReflectionsPS(), nullptr, 0);

			// set constant buffer
			ID3D11Buffer* CBs[] =
			{
				mRayTraced.GetReflectionsCB()
			};
			mContext->PSSetConstantBuffers(1, sizeof(CBs) / sizeof(CBs[0]), CBs);

			// draw
			mContext->Draw(3, 0);

			GPUProfilerTimestamp(TimestampQueryType::RayTracedReflections);
		}

		// set shader resource views
		ID3D11ShaderResourceView* pNullSRV = nullptr;
		mContext->PSSetShaderResources(2, 1, &pNullSRV);

		mContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	}
}

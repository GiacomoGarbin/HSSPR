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

	mCamera.SetPosition(0.0f, 0.0f, 0.0f);

	// build objects

	mBVH.Init(mDevice, mContext);
	mBVH.BuildBVH(mObjectManager.GetObjects(), mMeshManager);

	mRayTracedShadows.Init(mDevice, mContext);

	return true;
}

void AppInst::Update(const Timer& timer)
{
	AppBase::Update(timer);

	mRayTracedShadows.UpdateCB(mCamera.GetViewProjInvF(),
							   mLighting.GetLightDirection(0));
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

	// set shader resource views
	mContext->PSSetShaderResources(0, 1, mMaterialManager.GetAddressOfBufferSRV());

	for (std::size_t i = 0; i < mObjectManager.GetObjects().size(); ++i)
	{
		mObjectManager.UpdateBuffer(i);

		const MeshData& mesh = mMeshManager.GetMesh(mObjectManager.GetObject(i).mesh);

		// draw
		mContext->DrawIndexed(mesh.indexCount, mesh.indexStart, mesh.vertexBase);
	}

	// ray traced shadows
	{
		// set back buffer and depth stencil buffer
		mContext->OMSetRenderTargets(1,
									 mBackBufferRTV.GetAddressOf(),
									 nullptr);

		// set input layout
		mContext->IASetInputLayout(nullptr);

		// set vertex shader
		mContext->VSSetShader(mFullscreenVS.Get(), nullptr, 0);

		// set pixel shader
		mContext->PSSetShader(mRayTracedShadows.GetPixelShader(), nullptr, 0);

		// set constant buffer
		mContext->PSSetConstantBuffers(0, 1, mRayTracedShadows.GetAddressOfConstantBuffer());

		// set shader resource views
		ID3D11ShaderResourceView* SRVs[] =
		{
			mDepthBufferSRV.Get(),
			mBVH.GetBufferSRV()
		};

		mContext->PSSetShaderResources(0, sizeof(SRVs) / sizeof(SRVs[0]), SRVs);

		// set blend state
		mContext->OMSetBlendState(mRayTracedShadows.GetBlendState(), nullptr, 0xffffffff);

		// draw
		mContext->Draw(3, 0);

		// set shader resource views
		ID3D11ShaderResourceView* pNullSRV = nullptr;
		mContext->PSSetShaderResources(0, 1, &pNullSRV);


		mContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	}
}

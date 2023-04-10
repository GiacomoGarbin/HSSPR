#pragma once

// windows
#include <wrl.h>
#include <comdef.h>
using Microsoft::WRL::ComPtr;

// std
#include <string>

// d3d
#include <d3d11.h>
#include <directxmath.h>
using namespace DirectX;

// 
#include "Utility.h"

#define STRUCTURED 1

class RayTracedShadows
{
public:

	void Init(const ComPtr<ID3D11Device>& pDevice,
			  const ComPtr<ID3D11DeviceContext>& pContext)
	{
		mDevice = pDevice;
		mContext = pContext;

		// pixel shader
		{
			std::wstring path = L"shaders/RayTracedShadows.hlsl";

			const D3D_SHADER_MACRO defines[] =
			{
				"STRUCTURED", STRUCTURED ? "1" : "0",
				nullptr, nullptr
			};

			ComPtr<ID3DBlob> pCode = CompileShader(path,
												   defines,
												   "RayTracedShadowsPS",
												   ShaderTarget::PS);

			ThrowIfFailed(mDevice->CreatePixelShader(pCode->GetBufferPointer(),
													 pCode->GetBufferSize(),
													 nullptr,
													 &mPixelShader));

			NameResource(mPixelShader.Get(), "RayTracedShadowsPS");
		}

		// common constant buffer
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(CommonCB);
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			ThrowIfFailed(mDevice->CreateBuffer(&desc, nullptr, &mCommonCB));

			NameResource(mCommonCB.Get(), "RayTracedCommonCB");
		}

		// shadows constant buffer
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(ShadowsCB);
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			ThrowIfFailed(mDevice->CreateBuffer(&desc, nullptr, &mShadowsCB));

			NameResource(mShadowsCB.Get(), "RayTracedShadowsCB");
		}

		// blend state
		{
			D3D11_BLEND_DESC desc;
			desc.AlphaToCoverageEnable = false;
			desc.IndependentBlendEnable = false;
			desc.RenderTarget[0].BlendEnable = true;
			desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
			desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
			desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			ThrowIfFailed(mDevice->CreateBlendState(&desc, &mBlendState));

			NameResource(mBlendState.Get(), "RayTracedShadowsBS");
		}
	}

	void UpdateCBs(const XMFLOAT4X4& viewProjInv, const XMFLOAT3& lightDir)
	{
		CommonCB common;
		common.viewProjInv = viewProjInv;

		mContext->UpdateSubresource(mCommonCB.Get(), 0, nullptr, &common, 0, 0);

		ShadowsCB shadows;
		shadows.lightDir = XMFLOAT3(-lightDir.x, -lightDir.y, -lightDir.z);
		shadows.lightDirInv = XMFLOAT3(-1 / lightDir.x, -1 / lightDir.y, -1 / lightDir.z);

		mContext->UpdateSubresource(mShadowsCB.Get(), 0, nullptr, &shadows, 0, 0);
	}

	ID3D11PixelShader* GetPixelShader()
	{
		return mPixelShader.Get();
	}

	ID3D11Buffer* GetCommonCB()
	{
		return mCommonCB.Get();
	}

	ID3D11Buffer* GetShadowsCB()
	{
		return mShadowsCB.Get();
	}

	ID3D11BlendState* GetBlendState()
	{
		return mBlendState.Get();
	}

private:

	ComPtr<ID3D11Device> mDevice;
	ComPtr<ID3D11DeviceContext> mContext;

	ComPtr<ID3D11PixelShader> mPixelShader;
	ComPtr<ID3D11Buffer> mCommonCB;
	ComPtr<ID3D11Buffer> mShadowsCB;
	ComPtr<ID3D11BlendState> mBlendState;

	struct CommonCB
	{
		XMFLOAT4X4 viewProjInv;
	};

	static_assert((sizeof(CommonCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

	struct ShadowsCB
	{
		XMFLOAT3   lightDir;
		float      padding0;
		XMFLOAT3   lightDirInv;
		float      padding1;
	};

	static_assert((sizeof(ShadowsCB) % 16) == 0, "constant buffer size must be 16-byte aligned");
};


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

		// constant buffer
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(ConstantBuffer);
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			ThrowIfFailed(mDevice->CreateBuffer(&desc, nullptr, &mConstantBuffer));

			NameResource(mConstantBuffer.Get(), "RayTracedShadowsCB");
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

	void UpdateCB(const XMFLOAT4X4& viewProjInv, const XMFLOAT3& lightDir)
	{
		ConstantBuffer buffer;
		buffer.viewProjInv = viewProjInv;
		buffer.lightDir = lightDir;

		mContext->UpdateSubresource(mConstantBuffer.Get(), 0, nullptr, &buffer, 0, 0);
	}

	ID3D11PixelShader* GetPixelShader()
	{
		return mPixelShader.Get();
	}

	ID3D11Buffer** GetAddressOfConstantBuffer()
	{
		return mConstantBuffer.GetAddressOf();
	}

	ID3D11BlendState* GetBlendState()
	{
		return mBlendState.Get();
	}

private:

	ComPtr<ID3D11Device> mDevice;
	ComPtr<ID3D11DeviceContext> mContext;

	ComPtr<ID3D11PixelShader> mPixelShader;
	ComPtr<ID3D11Buffer> mConstantBuffer;

	struct ConstantBuffer
	{
		XMFLOAT4X4 viewProjInv;
		XMFLOAT3   lightDir;
		float      padding;
	};

	static_assert((sizeof(ConstantBuffer) % 16) == 0, "constant buffer size must be 16-byte aligned");

	ComPtr<ID3D11BlendState> mBlendState;
};


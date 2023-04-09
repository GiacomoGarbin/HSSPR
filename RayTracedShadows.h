#pragma once

// windows
#include <wrl.h>
#include <comdef.h>
using Microsoft::WRL::ComPtr;

// std
#include <string>

// d3d
#include <d3d11.h>

// 
#include "Utility.h"

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

			ComPtr<ID3DBlob> pCode = CompileShader(path,
												   nullptr,
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
	}

	void UpdateCB(const XMFLOAT3& lightDir)
	{
		ConstantBuffer buffer;
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

private:

	ComPtr<ID3D11Device> mDevice;
	ComPtr<ID3D11DeviceContext> mContext;

	ComPtr<ID3D11PixelShader> mPixelShader;
	ComPtr<ID3D11Buffer> mConstantBuffer;

	struct ConstantBuffer
	{
		XMFLOAT3 lightDir;
		float padding;
	};

	static_assert((sizeof(ConstantBuffer) % 16) == 0, "constant buffer size must be 16-byte aligned");
};


#pragma once

// std
#include <cassert>

// d3d
#include <d3d11.h>
#include <directxmath.h>
using namespace DirectX;

//
#include "MeshManager.h"
#include "ObjectManager.h"
#include "Utility.h"

class BVH
{
public:

	void Init(const ComPtr<ID3D11Device>& pDevice,
			  const ComPtr<ID3D11DeviceContext>& pContext)
	{
		mDevice = pDevice;
		mContext = pContext;
	}

	struct AABB
	{
		XMFLOAT4 min; // min.w = object
		XMFLOAT4 max; // max.w = offset
	};

	struct Node
	{
		static UINT count;

		Node()
		{
			count++;
		}

		~Node()
		{
			count--;
		}

		AABB aabb;
		Node* left;
		Node* right;
	};

	struct Leaf
	{
		XMFLOAT4 v0;
		XMFLOAT4 e1; // v1 - v0
		XMFLOAT4 e2; // v2 - v0
	};

	void BuildBVH(const std::vector<Object>& objects, const MeshManager& meshManager)
	{
		std::vector<AABB> boxes;
		std::vector<Leaf> leaves;

		for (const Object& object : objects)
		{
			const MeshData& mesh = meshManager.GetMesh(object.mesh);

			const std::size_t triangleCount = mesh.indexCount / 3;
			boxes.reserve(boxes.size() + triangleCount);
			leaves.reserve(leaves.size() + triangleCount);

			for (std::size_t i = 0; i < mesh.indexCount; i += 3)
			{
				const MeshData::IndexType i0 = mesh.indices[i + 0];
				const MeshData::IndexType i1 = mesh.indices[i + 1];
				const MeshData::IndexType i2 = mesh.indices[i + 2];

				// the w member of the returned XMVECTOR is initialized to 0
				XMVECTOR v0 = XMLoadFloat3(&mesh.vertices[i0].position);
				XMVECTOR v1 = XMLoadFloat3(&mesh.vertices[i1].position);
				XMVECTOR v2 = XMLoadFloat3(&mesh.vertices[i2].position);

				const XMMATRIX world = XMLoadFloat4x4(&object.world);

				// XMVector3Transform ignores the w component of the input vector, and uses a value of 1 instead
				v0 = XMVector3Transform(v0, world);
				v1 = XMVector3Transform(v1, world);
				v2 = XMVector3Transform(v2, world);

				AABB box;

				XMStoreFloat4(&box.min, XMVectorMin(v0, XMVectorMin(v1, v2)));
				XMStoreFloat4(&box.max, XMVectorMax(v0, XMVectorMax(v1, v2)));

				box.min.w = 0; // -1
				box.max.w = 0;

				boxes.push_back(box);

				Leaf leaf;

				XMStoreFloat4(&leaf.v0, v0);
				XMStoreFloat4(&leaf.e1, v1 - v0);
				XMStoreFloat4(&leaf.e2, v2 - v0);

				leaves.push_back(leaf);
			}
		}

		// create "vertex" buffer: copy leaves to buffer
		{}

		// BVH: AABB nodes tree
		{
			mRoot = CreateNode(boxes.data(), boxes.size());
		}

		WriteBVH();
	}

	void WriteBVH()
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(AABB) * Node::count;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(AABB);

		ThrowIfFailed(mDevice->CreateBuffer(&desc, nullptr, &mBuffer));
		NameResource(mBuffer.Get(), "BVH_NodeBuffer");

		ThrowIfFailed(mDevice->CreateShaderResourceView(mBuffer.Get(), nullptr, &mBufferSRV));
		NameResource(mBufferSRV.Get(), "BVH_NodeBufferSRV");

		D3D11_MAPPED_SUBRESOURCE mapped;
		mContext->Map(mBuffer.Get(), 0, D3D11_MAP_WRITE, 0, &mapped);

		AABB* buffer = static_cast<AABB*>(mapped.pData);

		int index = 0;
		WriteNode(mRoot, buffer, index);

		buffer[std::size_t(index - 1)].max.w = -1; // mark last node ?

		mContext->Unmap(mBuffer.Get(), 0);
	}

	void WriteNode(Node* node, AABB* buffer, int& index)
	{
		if (node)
		{
			buffer[index].min = node->aabb.min;
			buffer[index].max = node->aabb.max;

			if (IsNode(node))
			{
				int tempIndex = index++;

				WriteNode(node->left, buffer, index);
				WriteNode(node->right, buffer, index);

				// how many elements we need to skip to reach the right branch?
				buffer[tempIndex].max.w = float(tempIndex > 0 ? (index - tempIndex) : 1);
			}
			else
			{
				buffer[index].max.w = 1;
				index++;
			}
		}
	}

	//Node* CreateNode(std::vector<AABB>::iterator begin, std::vector<AABB>::iterator end)
	Node* CreateNode(AABB* boxes, std::size_t count)
	{
		assert(count > 0);

		XMVECTOR min = XMLoadFloat4(&boxes[0].min);
		XMVECTOR max = XMLoadFloat4(&boxes[0].max);

		for (std::size_t i = 1; i < count; ++i)
		{
			min = XMVectorMin(min, XMLoadFloat4(&boxes[i].min));
			max = XMVectorMax(max, XMLoadFloat4(&boxes[i].max));
		}

		AABB box;
		XMStoreFloat4(&box.min, min);
		XMStoreFloat4(&box.max, max);

		XMFLOAT3 size;
		XMStoreFloat3(&size, max - min);

		const float maxDim = max(size.x, max(size.y, size.z));

		if (maxDim == size.x)
		{
			std::qsort(boxes, count, sizeof(AABB), [](const void* lhs, const void* rhs)
			{
				const AABB* boxA = static_cast<const AABB*>(lhs);
				const AABB* boxB = static_cast<const AABB*>(rhs);

				float midPointA = 0.5f * (boxA->min.x + boxA->max.x);
				float midPointB = 0.5f * (boxB->min.x + boxB->max.x);

				return (midPointA < midPointB) ? -1 : +1;
			});
		}
		else if (maxDim == size.y)
		{
			std::qsort(boxes, count, sizeof(AABB), [](const void* lhs, const void* rhs)
			{
				const AABB* boxA = static_cast<const AABB*>(lhs);
				const AABB* boxB = static_cast<const AABB*>(rhs);

				float midPointA = 0.5f * (boxA->min.y + boxA->max.y);
				float midPointB = 0.5f * (boxB->min.y + boxB->max.y);

				return (midPointA < midPointB) ? -1 : +1;
			});
		}
		else
		{
			std::qsort(boxes, count, sizeof(AABB), [](const void* lhs, const void* rhs)
			{
				const AABB* boxA = static_cast<const AABB*>(lhs);
				const AABB* boxB = static_cast<const AABB*>(rhs);

				float midPointA = 0.5f * (boxA->min.z + boxA->max.z);
				float midPointB = 0.5f * (boxB->min.z + boxB->max.z);

				return (midPointA < midPointB) ? -1 : +1;
			});
		}

		Node* node = new Node();

		if (count == 1) // leaf
		{
			node->aabb = *boxes;
			node->left = nullptr;
			node->right = nullptr;

			MarkAsLeaf(node);
		}
		else
		{
			const size_t half = count / 2;

			node->left = CreateNode(boxes, half);
			node->right = CreateNode(boxes + half, count - half);

			XMStoreFloat4(&node->aabb.min, XMVectorMin(XMLoadFloat4(&node->left->aabb.min), XMLoadFloat4(&node->right->aabb.min)));
			XMStoreFloat4(&node->aabb.max, XMVectorMax(XMLoadFloat4(&node->left->aabb.max), XMLoadFloat4(&node->right->aabb.max)));

			MarkAsNode(node);
		}

		return node;
	}

private:

	void MarkAsNode(Node* node)
	{
		node->aabb.min.w = -1;
	}

	void MarkAsLeaf(Node* node)
	{
		node->aabb.min.w = +1;
	}

	bool IsNode(Node* node)
	{
		return node->aabb.min.w < 0;
	}

	// constant buffer
	// * viewProjInv to compute world position
	// * light dir for shadows

	// depth buffer SRV

	// bvh tree
	Node* mRoot = nullptr;

	ComPtr<ID3D11Device> mDevice;
	ComPtr<ID3D11DeviceContext> mContext;

	// bvh buffer (BVH nodes)
	ComPtr<ID3D11Buffer> mBuffer;
	ComPtr<ID3D11ShaderResourceView> mBufferSRV;

	// vertex data buffer (BVH leaves)
};


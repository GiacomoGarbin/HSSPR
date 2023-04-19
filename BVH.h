#pragma once

// std
#include <cassert>

// d3d
#include <d3d11.h>
#include <directxmath.h>
using namespace DirectX;

//
#include "MaterialManager.h"
#include "MeshManager.h"
#include "ObjectManager.h"
#include "RayTraced.h"
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
		XMVECTOR min;
		XMVECTOR max;
		XMVECTOR mid;

		AABB()
			: mid(XMVectorZero())
			//, min(XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.0f))
			//, max(XMVectorSet(+FLT_MAX, +FLT_MAX, +FLT_MAX, 0.0f))
		{
			min = XMVectorSet(+FLT_MAX, +FLT_MAX, +FLT_MAX, 0.0f);
			max = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.0f);
		}

		void Expand(const XMVECTOR& point)
		{
			min = XMVectorMin(min, point);
			max = XMVectorMax(max, point);
			mid = 0.5f * (min + max);
		}

		void Expand(const AABB& aabb)
		{
			min = XMVectorMin(min, aabb.min);
			max = XMVectorMax(max, aabb.max);
			mid = 0.5f * (min + max);
		}
	};

	struct Node
	{
		XMFLOAT4 min; // min.w = bIsNode
		XMFLOAT4 max;
	};

	struct Leaf
	{
		XMFLOAT4 v0;
		XMFLOAT4 e1; // v1 - v0
		XMFLOAT4 e2; // v2 - v0
	};

	struct TriangleData
	{
		AABB aabb;

		XMVECTOR v0;
		XMVECTOR v1;
		XMVECTOR v2;

		std::size_t offset;
		std::size_t material;

		float surfaceAreaLeft;
		float surfaceAreaRight;
	};

	struct TreeNode
	{
		TriangleData data;

		bool bIsNode; // otherwise leaf

		float splitCost;

		TreeNode* left;
		TreeNode* right;
	};

	void BuildBVH(const std::vector<Object>& objects,
				  const MeshManager& meshManager,
				  const MaterialManager& materialManager)
	{
		std::vector<TriangleData> triangles;
		std::size_t offset = 0; // triangle offset in vertex buffer

		std::vector<XMFLOAT4> vertices;

		for (std::size_t j = 0; j < objects.size() - 1; ++j)
		{
			const Object& object = objects[j];
			const MeshData& mesh = meshManager.GetMesh(object.mesh);
			const XMMATRIX world = XMLoadFloat4x4(&object.world);
			const Material& material = materialManager.GetMaterial(object.material);
			const XMMATRIX uvTransform = XMLoadFloat4x4(&object.uvTransform) * XMLoadFloat4x4(&material.uvTransform);

			const std::size_t indexCount = mesh.indexCount ? mesh.indexCount : mesh.vertices.size();

			vertices.reserve(vertices.size() + indexCount * 2);
			triangles.reserve(triangles.size() + (indexCount / 3));

			for (std::size_t i = 0; i < indexCount; i += 3)
			{
				const MeshData::IndexType i0 = mesh.indexCount ? mesh.indices[i + 0] : MeshData::IndexType(i + 0);
				const MeshData::IndexType i1 = mesh.indexCount ? mesh.indices[i + 1] : MeshData::IndexType(i + 1);
				const MeshData::IndexType i2 = mesh.indexCount ? mesh.indices[i + 2] : MeshData::IndexType(i + 2);

				// the w member of the returned XMVECTOR is initialized to 0
				XMVECTOR v0 = XMLoadFloat3(&mesh.vertices[i0].position);
				XMVECTOR v1 = XMLoadFloat3(&mesh.vertices[i1].position);
				XMVECTOR v2 = XMLoadFloat3(&mesh.vertices[i2].position);

				// XMVector3Transform ignores the w component of the input vector, and uses a value of 1 instead
				v0 = XMVector3Transform(v0, world);
				v1 = XMVector3Transform(v1, world);
				v2 = XMVector3Transform(v2, world);

				TriangleData triangle;

				triangle.offset = offset;
				offset += 3 * 2;

				triangle.material = object.material;

				triangle.aabb.Expand(v0);
				triangle.aabb.Expand(v1);
				triangle.aabb.Expand(v2);

				triangle.aabb.min *= XMVectorSet(1, 1, 1, 0);
				triangle.aabb.max *= XMVectorSet(1, 1, 1, 0);

				triangle.v0 = v0;
				triangle.v1 = v1;
				triangle.v2 = v2;

				triangles.push_back(triangle);

				// vertex buffer
				{
					XMVECTOR n0 = XMLoadFloat3(&mesh.vertices[i0].normal);
					XMVECTOR n1 = XMLoadFloat3(&mesh.vertices[i1].normal);
					XMVECTOR n2 = XMLoadFloat3(&mesh.vertices[i2].normal);

					n0 = XMVector3TransformNormal(n0, world);
					n1 = XMVector3TransformNormal(n1, world);
					n2 = XMVector3TransformNormal(n2, world);

					XMVECTOR u0 = XMLoadFloat2(&mesh.vertices[i0].uv);
					XMVECTOR u1 = XMLoadFloat2(&mesh.vertices[i1].uv);
					XMVECTOR u2 = XMLoadFloat2(&mesh.vertices[i2].uv);

					//u0 = XMVector2Transform(u0, uvTransform);
					//u1 = XMVector2Transform(u1, uvTransform);
					//u2 = XMVector2Transform(u2, uvTransform);

					XMVECTOR t0 = XMLoadFloat3(&mesh.vertices[i0].tangent);
					XMVECTOR t1 = XMLoadFloat3(&mesh.vertices[i1].tangent);
					XMVECTOR t2 = XMLoadFloat3(&mesh.vertices[i2].tangent);

					t0 = XMVector3TransformNormal(t0, world);
					t1 = XMVector3TransformNormal(t1, world);
					t2 = XMVector3TransformNormal(t2, world);

					XMFLOAT4 a, b;

					XMStoreFloat4(&a, n0);
					a.w = XMVectorGetX(u0);
					vertices.push_back(a);

					XMStoreFloat4(&b, t0);
					b.w = XMVectorGetY(u0);
					vertices.push_back(b);

					XMStoreFloat4(&a, n1);
					a.w = XMVectorGetX(u1);
					vertices.push_back(a);

					XMStoreFloat4(&b, t1);
					b.w = XMVectorGetY(u1);
					vertices.push_back(b);

					XMStoreFloat4(&a, n2);
					a.w = XMVectorGetX(u2);
					vertices.push_back(a);

					XMStoreFloat4(&b, t2);
					b.w = XMVectorGetY(u2);
					vertices.push_back(b);
				}
			}
		}

		// vertex buffer
		WriteVertexBuffer(vertices);

		TreeNode* root = CreateTreeNode(triangles, 0, int(triangles.size() - 1));

		//PrintTreeNode(root);

		const int kMaxNodeCount = 1000000;
		uint8_t* data = new uint8_t[kMaxNodeCount * sizeof(Leaf)];

		int dataOffset = 0;
		WriteNode(root, root, data, dataOffset);

		// terminate tree
		Node* node = reinterpret_cast<Node*>(data + dataOffset);
		node->min.w = 0;

		dataOffset += sizeof(Node);

		assert(dataOffset <= kMaxNodeCount);

		WriteTreeToBuffer(data, dataOffset);

		delete[] data;
		DeleteTreeNode(root);
	}

	void PrintTreeNode(TreeNode* node, int level = 0)
	{
		auto Float3ToStr = [](const XMFLOAT3& f3)
		{
			std::stringstream ss;
			ss << "(" << f3.x << "," << f3.y << "," << f3.z << ")";
			return ss.str();
		};

		if (node)
		{
			XMFLOAT3 min, max;
			XMStoreFloat3(&min, node->data.aabb.min);
			XMStoreFloat3(&max, node->data.aabb.max);

			std::stringstream ss;

			ss << !node->bIsNode << " " << std::string(level, '.') << " AABB min=" << Float3ToStr(min) << " max=" << Float3ToStr(max) << "\n";
			
			if (!node->bIsNode)
			{
				XMFLOAT3 v0, v1, v2;
				XMStoreFloat3(&v0, node->data.v0);
				XMStoreFloat3(&v1, node->data.v1);
				XMStoreFloat3(&v2, node->data.v2);

				ss << "  " << std::string(level, '.') << " v0=" << Float3ToStr(v0) << " v1=" << Float3ToStr(v1) << " v2=" << Float3ToStr(v2) << "\n";
			}

			OutputDebugStringA(ss.str().c_str());

			PrintTreeNode(node->left, level + 1);
			PrintTreeNode(node->right, level + 1);
		}
	}

	ID3D11ShaderResourceView* GetTreeBufferSRV()
	{
		return mTreeBufferSRV.Get();
	}

	ID3D11ShaderResourceView* GetVertexBufferSRV()
	{
		return mVertexBufferSRV.Get();
	}

private:

	TreeNode* CreateTreeNode(std::vector<TriangleData>& triangles, const int begin, const int end)
	{
		int count = end - begin + 1;
		assert(count > 0);

		TreeNode* node = new TreeNode();

		for (std::size_t i = begin; i <= end; ++i)
		{
			node->data.aabb.Expand(triangles[i].aabb);
		}

		if (count == 1) // leaf
		{
			node->left = nullptr;
			node->right = nullptr;

			node->bIsNode = false;

			node->data.offset = triangles[begin].offset;
			node->data.material = triangles[begin].material;

			node->data.v0 = triangles[begin].v0;
			node->data.v1 = triangles[begin].v1;
			node->data.v2 = triangles[begin].v2;
		}
		else // node
		{
			int split;
			Axis axis;
			float cost;

			FindBestSplit(triangles, begin, end, split, axis, cost);

			SortAlongAxis(triangles, begin, end, axis);

			node->left = CreateTreeNode(triangles, begin, split - 1);
			node->right = CreateTreeNode(triangles, split, end);

			// access the child with the largest probability of collision first
			const float surfaceAreaLeft = CalculateSurfaceArea(node->left->data.aabb);
			const float surfaceAreaRight = CalculateSurfaceArea(node->right->data.aabb);

			if (surfaceAreaRight > surfaceAreaLeft)
			{
				std::swap(node->left, node->right);
			}

			node->bIsNode = true;
		}

		return node;
	}

	void DeleteTreeNode(TreeNode* node)
	{
		if (node)
		{
			DeleteTreeNode(node->left);
			DeleteTreeNode(node->right);

			delete node;
		}
	}

	enum class Axis
	{
		X,
		Y,
		Z,
	};

	void FindBestSplit(std::vector<TriangleData>& triangles, const int begin, const int end, int& split, Axis& splitAxis, float& splitCost)
	{
		split = begin;
		splitAxis = Axis::X;
		splitCost = FLT_MAX;

		const int count = end - begin + 1;
		int bestSplit = begin;

		for (int i = 0; i < 3; ++i)
		{
			Axis axis = Axis(i);

			SortAlongAxis(triangles, begin, end, axis);

			AABB aabbLeft;
			AABB aabbRight;

			for (int indexLeft = 0; indexLeft < count; ++indexLeft)
			{
				int indexRight = count - indexLeft - 1;

				aabbLeft.Expand(triangles[begin + indexLeft].aabb);
				aabbRight.Expand(triangles[begin + indexRight].aabb);

				float surfaceAreaLeft = CalculateSurfaceArea(aabbLeft);
				float surfaceAreaRight = CalculateSurfaceArea(aabbRight);

				triangles[begin + indexLeft].surfaceAreaLeft = surfaceAreaLeft;
				triangles[begin + indexRight].surfaceAreaRight = surfaceAreaRight;
			}

			float bestCost = FLT_MAX;

			for (int mid = begin + 1; mid <= end; ++mid)
			{
				float surfaceAreaLeft = triangles[mid - 1].surfaceAreaLeft;
				float surfaceAreaRight = triangles[mid].surfaceAreaRight;

				int countLeft = mid - begin;
				int countRight = end - mid;

				float costLeft = surfaceAreaLeft * (float)countLeft;
				float costRight = surfaceAreaRight * (float)countRight;

				float cost = costLeft + costRight;

				if (cost < bestCost)
				{
					bestSplit = mid;
					bestCost = cost;
				}
			}

			if (bestCost < splitCost)
			{
				split = bestSplit;
				splitAxis = axis;
				splitCost = bestCost;
			}
		}
	}

	void SortAlongAxis(std::vector<TriangleData>& data, const int begin, const int end, const Axis axis)
	{
		TriangleData* base = data.data() + begin;
		const int count = end - begin + 1;
		const std::size_t size = sizeof(TriangleData);

		switch (axis)
		{
			case Axis::X:
				std::qsort(base, count, size, [](const void* lhs, const void* rhs)
				{
					const TriangleData* a = static_cast<const TriangleData*>(lhs);
					const TriangleData* b = static_cast<const TriangleData*>(rhs);

					return (XMVectorGetX(a->aabb.mid) < XMVectorGetX(b->aabb.mid)) ? -1 : +1;
				});
				break;
			case Axis::Y:
				std::qsort(base, count, size, [](const void* lhs, const void* rhs)
				{
					const TriangleData* a = static_cast<const TriangleData*>(lhs);
					const TriangleData* b = static_cast<const TriangleData*>(rhs);

					return (XMVectorGetY(a->aabb.mid) < XMVectorGetY(b->aabb.mid)) ? -1 : +1;
				});
				break;
			case Axis::Z:
				std::qsort(base, count, size, [](const void* lhs, const void* rhs)
				{
					const TriangleData* a = static_cast<const TriangleData*>(lhs);
					const TriangleData* b = static_cast<const TriangleData*>(rhs);

					return (XMVectorGetZ(a->aabb.mid) < XMVectorGetZ(b->aabb.mid)) ? -1 : +1;
				});
				break;
		}
	}

	inline float CalculateSurfaceArea(const AABB& aabb)
	{
		XMFLOAT3 extents;
		XMStoreFloat3(&extents, aabb.max - aabb.min); // abs ?

		return (extents.x * extents.y + extents.y * extents.z + extents.z * extents.x) * 2.0f;
	}

	void WriteNode(TreeNode* root, TreeNode* treeNode, uint8_t* data, int& dataOffset)
	{
		if (treeNode)
		{
			if (treeNode->bIsNode)
			{
				int tempDataOffset = 0;
				Node* node = nullptr;

				// do not write the root node, for secondary rays the origin will always be in the scene bounding box
				if (treeNode != root)
				{
					node = reinterpret_cast<Node*>(data + dataOffset);

					XMStoreFloat4(&node->min, treeNode->data.aabb.min);
					XMStoreFloat4(&node->max, treeNode->data.aabb.max);

					dataOffset += sizeof(Node);

					tempDataOffset = dataOffset;
				}

				WriteNode(root, treeNode->left, data, dataOffset);
				WriteNode(root, treeNode->right, data, dataOffset);

				if (treeNode != root)
				{
					// when on the left branch, how many float4 elements we need to skip to reach the right branch?
					node->min.w = -float(dataOffset - tempDataOffset) / sizeof(XMFLOAT4);
				}
			}
			else // leaf
			{
				Leaf* leaf = reinterpret_cast<Leaf*>(data + dataOffset);

				XMStoreFloat4(&leaf->v0, treeNode->data.v0);
				XMStoreFloat4(&leaf->e1, treeNode->data.v1 - treeNode->data.v0);
				XMStoreFloat4(&leaf->e2, treeNode->data.v2 - treeNode->data.v0);

				// when on the left branch, how many float4 elements we need to skip to reach the right branch?
				leaf->v0.w = sizeof(Leaf) / sizeof(XMFLOAT4);

				leaf->e1.w = float(treeNode->data.offset); // triangle index to fetch vertex data
				leaf->e2.w = float(treeNode->data.material); // material index

				dataOffset += sizeof(Leaf);
			}
		}
	}

	void WriteTreeToBuffer(uint8_t* data, const int size)
	{
#if STRUCTURED
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = size;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(XMFLOAT4);
#else
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = size;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		desc.StructureByteStride = 0;
#endif

		D3D11_SUBRESOURCE_DATA initData;
		initData.pSysMem = data;

		ThrowIfFailed(mDevice->CreateBuffer(&desc, &initData, &mTreeBuffer));
		NameResource(mTreeBuffer.Get(), "BVHTreeBuffer");

#if STRUCTURED
		ThrowIfFailed(mDevice->CreateShaderResourceView(mTreeBuffer.Get(), nullptr, &mTreeBufferSRV));
		NameResource(mTreeBufferSRV.Get(), "BVHTreeBufferSRV");
#else
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC desc;
			desc.Format = DXGI_FORMAT_R32_TYPELESS;
			desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			desc.BufferEx.FirstElement = 0;
			desc.BufferEx.NumElements = size / sizeof(float);
			desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;

			ThrowIfFailed(mDevice->CreateShaderResourceView(mTreeBuffer.Get(), &desc, &mTreeBufferSRV));
			NameResource(mTreeBufferSRV.Get(), "BVHTreeBufferSRV");
		}
#endif

		//D3D11_MAPPED_SUBRESOURCE mapped;
		//mContext->Map(mTreeBuffer.Get(), 0, D3D11_MAP_WRITE, 0, &mapped);

		//AABB* buffer = static_cast<AABB*>(mapped.pData);

		//int index = 0;
		//WriteNode(mRoot, buffer, index);

		//buffer[std::size_t(index - 1)].max.w = -1; // mark last node ?

		//mContext->Unmap(mTreeBuffer.Get(), 0);
	}

	void WriteVertexBuffer(const std::vector<XMFLOAT4>& vertices)
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = UINT(vertices.size()) * sizeof(XMFLOAT4);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(XMFLOAT4);

		D3D11_SUBRESOURCE_DATA initData;
		initData.pSysMem = vertices.data();

		ThrowIfFailed(mDevice->CreateBuffer(&desc, &initData, &mVertexBuffer));
		NameResource(mVertexBuffer.Get(), "BVHVertexBuffer");

		ThrowIfFailed(mDevice->CreateShaderResourceView(mVertexBuffer.Get(), nullptr, &mVertexBufferSRV));
		NameResource(mVertexBufferSRV.Get(), "BVHVertexBufferSRV");
	}

	ComPtr<ID3D11Device> mDevice;
	ComPtr<ID3D11DeviceContext> mContext;

	ComPtr<ID3D11Buffer> mTreeBuffer;
	ComPtr<ID3D11ShaderResourceView> mTreeBufferSRV;

	ComPtr<ID3D11Buffer> mVertexBuffer;
	ComPtr<ID3D11ShaderResourceView> mVertexBufferSRV;
};
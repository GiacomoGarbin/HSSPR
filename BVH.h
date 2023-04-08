#pragma once

// d3d
#include <d3d11.h>
#include <directxmath.h>
using namespace DirectX;

//
#include "MeshManager.h"
#include "ObjectManager.h"

class BVH
{
public:

	void BuildBVH(const std::vector<Object>& objects, const MeshManager& meshManager)
	{
		// for each object
		//   for each triangle
		//     transform to world space -> BVH::Leaf
		//     compute AABB             -> BVH::Node

		for (const Object& object : objects)
		{
			const MeshData& mesh = meshManager.GetMesh(object.mesh);

			for (std::size_t i = 0; i < mesh.indices.size(); i += 3)
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

				BVH::Node node;

				XMStoreFloat4(&node.min, XMVectorMin(v0, XMVectorMin(v1, v2)));
				XMStoreFloat4(&node.min, XMVectorMax(v0, XMVectorMax(v1, v2)));

				//node.min.x = min(XMVectorGetX(v0), min(XMVectorGetX(v1), XMVectorGetX(v2)));
				//node.min.y = min(XMVectorGetY(v0), min(XMVectorGetY(v1), XMVectorGetY(v2)));
				//node.min.z = min(XMVectorGetZ(v0), min(XMVectorGetZ(v1), XMVectorGetZ(v2)));

				//node.max.x = max(XMVectorGetX(v0), max(XMVectorGetX(v1), XMVectorGetX(v2)));
				//node.max.y = max(XMVectorGetY(v0), max(XMVectorGetY(v1), XMVectorGetY(v2)));
				//node.max.z = max(XMVectorGetZ(v0), max(XMVectorGetZ(v1), XMVectorGetZ(v2)));

#if 0
				node.object = -1;
				node.sibling = -1;
#else
				//node.min.w = 0;
#endif

				BVH::Leaf leaf;

				XMStoreFloat4(&leaf.v0, v0);
				XMStoreFloat4(&leaf.e1, v1 - v0);
				XMStoreFloat4(&leaf.e2, v2 - v0);

				//leaf.v0.w = object;
			}
		}
	}

	struct Node
	{
#if 0
		XMFLOAT3 min;
		UINT object;
		XMFLOAT3 max;
		UINT sibling;
#else
		XMFLOAT4 min; // min.w = object
		XMFLOAT4 max; // max.w = sibling
#endif
	};

	struct Leaf
	{
		XMFLOAT4 v0;
		XMFLOAT4 e1; // v1 - v0
		XMFLOAT4 e2; // v2 - v0
	};

private:

	// bvh tree
	// bvh buffer
	// vertex data buffer
};


#pragma once

// std
#include <vector>

// d3d
#include <directxmath.h>
using namespace DirectX;

class GeometryGenerator
{
public:
	struct VertexData
	{
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT3 tangent;
		XMFLOAT2 uv;

		VertexData();
		VertexData(float px, float py, float pz,
				   float nx, float ny, float nz,
				   float u,  float v,
				   float tx, float ty, float tz);
	};

	using IndexType = uint16_t;

	struct MeshData
	{
		std::vector<VertexData> vertices;
		std::vector<IndexType> indices;
	};

	static MeshData CreateBox(const float width, const float height, const float depth);
};
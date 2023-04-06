#include "GeometryGenerator.h"

GeometryGenerator::VertexData::VertexData()
	: position(0, 0, 0)
	, normal(0, 0, 0)
	, uv(0, 0)
	, tangent(0, 0, 0)
{}

GeometryGenerator::VertexData::VertexData(float px, float py, float pz,
										  float nx, float ny, float nz,
										  float u,  float v,
										  float tx, float ty, float tz)
	: position(px, py, pz)
	, normal(nx, ny, nz)
	, uv(u, v)
	, tangent(tx, ty, tz)
{}

GeometryGenerator::MeshData GeometryGenerator::CreateBox(float width, float height, float depth)
{
	MeshData mesh;

	// half extents
	const float w = 0.5f * width;
	const float h = 0.5f * height;
	const float d = 0.5f * depth;

	std::vector<VertexData>& vertices = mesh.vertices;
	vertices.resize(24);
	
	// front
	vertices[ 0] = VertexData(-w, -h, -d,  0,  0, -1,  0, +1, +1,  0,  0);
	vertices[ 1] = VertexData(-w, +h, -d,  0,  0, -1,  0,  0, +1,  0,  0);
	vertices[ 2] = VertexData(+w, +h, -d,  0,  0, -1, +1,  0, +1,  0,  0);
	vertices[ 3] = VertexData(+w, -h, -d,  0,  0, -1, +1, +1, +1,  0,  0);

	// back
	vertices[ 4] = VertexData(-w, -h, +d,  0,  0, +1, +1, +1, -1,  0,  0);
	vertices[ 5] = VertexData(+w, -h, +d,  0,  0, +1,  0, +1, -1,  0,  0);
	vertices[ 6] = VertexData(+w, +h, +d,  0,  0, +1,  0,  0, -1,  0,  0);
	vertices[ 7] = VertexData(-w, +h, +d,  0,  0, +1, +1,  0, -1,  0,  0);

	// top
	vertices[ 8] = VertexData(-w, +h, -d,  0, +1,  0,  0, +1, +1,  0,  0);
	vertices[ 9] = VertexData(-w, +h, +d,  0, +1,  0,  0,  0, +1,  0,  0);
	vertices[10] = VertexData(+w, +h, +d,  0, +1,  0, +1,  0, +1,  0,  0);
	vertices[11] = VertexData(+w, +h, -d,  0, +1,  0, +1, +1, +1,  0,  0);

	// bottom
	vertices[12] = VertexData(-w, -h, -d,  0, -1,  0, +1, +1, -1,  0,  0);
	vertices[13] = VertexData(+w, -h, -d,  0, -1,  0,  0, +1, -1,  0,  0);
	vertices[14] = VertexData(+w, -h, +d,  0, -1,  0,  0,  0, -1,  0,  0);
	vertices[15] = VertexData(-w, -h, +d,  0, -1,  0, +1,  0, -1,  0,  0);

	// left
	vertices[16] = VertexData(-w, -h, +d, -1,  0,  0,  0, +1,  0,  0, -1);
	vertices[17] = VertexData(-w, +h, +d, -1,  0,  0,  0,  0,  0,  0, -1);
	vertices[18] = VertexData(-w, +h, -d, -1,  0,  0, +1,  0,  0,  0, -1);
	vertices[19] = VertexData(-w, -h, -d, -1,  0,  0, +1, +1,  0,  0, -1);

	// right
	vertices[20] = VertexData(+w, -h, -d, +1,  0,  0,  0, +1,  0,  0, +1);
	vertices[21] = VertexData(+w, +h, -d, +1,  0,  0,  0,  0,  0,  0, +1);
	vertices[22] = VertexData(+w, +h, +d, +1,  0,  0, +1,  0,  0,  0, +1);
	vertices[23] = VertexData(+w, -h, +d, +1,  0,  0, +1, +1,  0,  0, +1);

	std::vector<uint16_t>& indices = mesh.indices;
	indices.resize(36);

	// front
	indices[ 0] =  0; indices[ 1] =  1; indices[ 2] =  2;
	indices[ 3] =  0; indices[ 4] =  2; indices[ 5] =  3;

	// back
	indices[ 6] =  4; indices[ 7] =  5; indices[ 8] =  6;
	indices[ 9] =  4; indices[10] =  6; indices[11] =  7;

	// top
	indices[12] =  8; indices[13] =  9; indices[14] = 10;
	indices[15] =  8; indices[16] = 10; indices[17] = 11;

	// bottom
	indices[18] = 12; indices[19] = 13; indices[20] = 14;
	indices[21] = 12; indices[22] = 14; indices[23] = 15;

	// left
	indices[24] = 16; indices[25] = 17; indices[26] = 18;
	indices[27] = 16; indices[28] = 18; indices[29] = 19;

	// right
	indices[30] = 20; indices[31] = 21; indices[32] = 22;
	indices[33] = 20; indices[34] = 22; indices[35] = 23;

	return mesh;
}
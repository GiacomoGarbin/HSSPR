#include "../RenderToyD3D11/shaders/Fullscreen.hlsl"

cbuffer RayTracedCommonCB : register(b0)
{
    float4x4 viewProjInv;
};

#if NORMALS
Texture2D<float4> DepthNormalBuffer : register(t0);
#else
Texture2D<float> DepthBuffer : register(t0);
#endif

#if STRUCTURED
StructuredBuffer<float4> BVH : register(t1);
#else
ByteAddressBuffer BVH : register(t1);
#endif

#define kEpsilon 0.00001f

bool RayBoxIntersect(const float3 origin,
                     const float3 dirInv,
                     const float3 aabbMin,
                     const float3 aabbMax)
{
	const float3 t0 = (aabbMin - origin) * dirInv;
	const float3 t1 = (aabbMax - origin) * dirInv;

	const float3 tmin = min(t0, t1);
	const float3 tmax = max(t0, t1);

    const float a0 = max(max(0.0f, tmin.x), max(tmin.y, tmin.z));
	const float a1 = min(tmax.x, min(tmax.y, tmax.z));
    
	return a1 >= a0;
}

bool RayTriIntersect(const float3 origin,
                     const float3 dir,
                     const float3 v0,
                     const float3 e1, // v1 - v0
                     const float3 e2, // v2 - v0
                     inout float t,
                     inout float2 barycentricCoords)
{
	const float3 s1 = cross(dir.xyz, e2);
	const float  invd = 1.0 / (dot(s1, e1));
	const float3 d = origin.xyz - v0;
	barycentricCoords.x = dot(d, s1) * invd;
	const float3 s2 = cross(d, e1);
	barycentricCoords.y = dot(dir.xyz, s2) * invd;
	t = dot(e2, s2) * invd;

	if (
#if BACKFACE_CULLING
		dot(s1, e1) < -kEpsilon ||
#endif
		barycentricCoords.x < 0.0 || barycentricCoords.x > 1.0 || barycentricCoords.y < 0.0 || (barycentricCoords.x + barycentricCoords.y) > 1.0 || t < 0.0 || t > 1e9)
	{
		return false;
	}
	else
	{
		return true;
	}
}
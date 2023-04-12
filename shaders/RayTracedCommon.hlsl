#define FIXME 1
#include "../RenderToyD3D11/shaders/Common.hlsl"
#include "../RenderToyD3D11/shaders/Fullscreen.hlsl"

#if NORMALS
Texture2D<float4> DepthNormalBuffer : register(t2);
#else
Texture2D<float> DepthBuffer : register(t2);
#endif

#if STRUCTURED
StructuredBuffer<float4> BVH : register(t3);
#else
ByteAddressBuffer BVH : register(t3);
#endif

#define kEpsilon 0.00001f

#if !SHADOWS
// #define FLT_MAX 3.402823466e+38f
#define FLT_MAX 1000000000
#endif

float3 GetWorldPos(const float2 uv, const float depth)
{
    float4 clipPos = float4(2 * uv - 1, depth, 1);
    clipPos.y = -clipPos.y;

    float4 worldPos = mul(viewProjInv, clipPos);
    worldPos.xyz /= worldPos.w;

	return worldPos.xyz;
}

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

bool RayTraced(const float3 worldPos,
			   const float3 rayDir,
			   const float3 rayDirInv,
#if SHADOWS
			   inout float t,
			   inout float2 barycentricCoords)
#else
			   inout int material,
			   inout float3 normal)
#endif
{
	bool collision = false;
    int dataOffset = 0;
	int offsetToNextNode = 1;
	
#if !SHADOWS
	float minDist = FLT_MAX;
	bool hit = false;

	float t = 0;
	float2 barycentricCoords = 0;

	// int material = -1;
	// float3 normal = 0;
#endif

    [loop]
    while (offsetToNextNode != 0)
    {
        float4 element0 = BVH[dataOffset++];
        float4 element1 = BVH[dataOffset++];

        offsetToNextNode = int(element0.w);

        collision = false;

        if (offsetToNextNode < 0) // node
        {
            // check for intersection with node AABB
            collision = RayBoxIntersect(worldPos, rayDirInv, element0.xyz, element1.xyz);

            // if there is collision, go to the next node (left) 
            if (!collision)
            {
                // or else skip over the whole branch (right)
                dataOffset += abs(offsetToNextNode);
            }
        }
        else if (offsetToNextNode > 0) // leaf
        {
            float4 element2 = BVH[dataOffset++];

            // check for intersection with leaf triangle
            collision = RayTriIntersect(worldPos, rayDir, element0.xyz, element1.xyz, element2.xyz, t, barycentricCoords);

#if SHADOWS
            if (collision)
            {
                break;
            }
#else
            if (collision && t < minDist)
            {
				minDist = t;
				hit = true;

				material = element2.w;
				normal = normalize(cross(element1.xyz, element2.xyz));
            }
#endif
        }
    }

#if !SHADOWS
	t = material;
#endif

#if SHADOWS
	return collision;
#else
	return hit;
#endif
}
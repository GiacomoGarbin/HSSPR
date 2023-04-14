#define FIXME 1
#include "../RenderToyD3D11/shaders/Common.hlsl"
#include "../RenderToyD3D11/shaders/Fullscreen.hlsl"

#if NORMALS
Texture2D<float4> NormalDepthBuffer : register(t2);
#else
Texture2D<float> DepthBuffer : register(t2);
#endif

#if STRUCTURED
StructuredBuffer<float4> BVH : register(t3);
#else
ByteAddressBuffer BVH : register(t3);
#endif

#if !SHADOWS
#if STRUCTURED
StructuredBuffer<float4>
#else
ByteAddressBuffer
#endif
VertexBuffer : register(t4);
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
                     inout float2 bc)
{
	const float3 s1 = cross(dir.xyz, e2);
	const float  invd = 1.0 / (dot(s1, e1));
	const float3 d = origin.xyz - v0;
	bc.x = dot(d, s1) * invd;
	const float3 s2 = cross(d, e1);
	bc.y = dot(dir.xyz, s2) * invd;
	t = dot(e2, s2) * invd;

	if (
#if BACKFACE_CULLING
		dot(s1, e1) < -kEpsilon ||
#endif
		bc.x < 0.0 || bc.x > 1.0 || bc.y < 0.0 || (bc.x + bc.y) > 1.0 || t < 0.0 || t > 1e9)
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
			   inout float2 bc)
#else
			   inout int material,
               inout float3 hitWorldPos,
			   inout float3 normal,
			   inout float2 uv,
			   inout float3 tangent)
#endif
{
	bool collision = false;
    int dataOffset = 0;
	int offsetToNextNode = 1;
	
#if !SHADOWS
	float minDist = FLT_MAX;
	bool hit = false;

	float t = 0;
	float2 bc = 0; // barycentric coords
#endif

    [loop]
    while (offsetToNextNode != 0)
    {
        const float4 element0 = BVH[dataOffset++];
        const float4 element1 = BVH[dataOffset++];

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
            const float4 element2 = BVH[dataOffset++];

            // check for intersection with leaf triangle
            collision = RayTriIntersect(worldPos, rayDir, element0.xyz, element1.xyz, element2.xyz, t, bc);

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

                const float3 v0 = element0.xyz;
                const float3 v1 = element1.xyz + v0;
                const float3 v2 = element2.xyz + v0;

                // triangle index
                int offset = element1.w;
                
                float4 e0 = VertexBuffer[offset++];
                float4 e1 = VertexBuffer[offset++];

                const float3 n0 = e0.xyz;
                const float3 t0 = e1.xyz;
                const float2 u0 = float2(e0.w, e1.w);
                
                e0 = VertexBuffer[offset++];
                e1 = VertexBuffer[offset++];

                const float3 n1 = e0.xyz;
                const float3 t1 = e1.xyz;
                const float2 u1 = float2(e0.w, e1.w);
                
                e0 = VertexBuffer[offset++];
                e1 = VertexBuffer[offset++];

                const float3 n2 = e0.xyz;
                const float3 t2 = e1.xyz;
                const float2 u2 = float2(e0.w, e1.w);

				material    = element2.w;
                hitWorldPos = v0 * (1 - bc.x - bc.y) + v1 * bc.x + v2 * bc.y;
                normal      = n0 * (1 - bc.x - bc.y) + n1 * bc.x + n2 * bc.y;
                uv          = u0 * (1 - bc.x - bc.y) + u1 * bc.x + u2 * bc.y;
                tangent     = t0 * (1 - bc.x - bc.y) + t1 * bc.x + t2 * bc.y;
            }
#endif
        }
    }

#if SHADOWS
	return collision;
#else
	return hit;
#endif
}
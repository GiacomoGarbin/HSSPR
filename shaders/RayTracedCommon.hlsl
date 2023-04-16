#define FIXME 1
#include "../RenderToyD3D11/shaders/Common.hlsl"
#include "../RenderToyD3D11/shaders/Fullscreen.hlsl"

Texture2D<float> DepthBuffer : register(t3);
Texture2D<float4> GBuffer : register(t4);

#if STRUCTURED
StructuredBuffer<float4>
#else
ByteAddressBuffer
#endif
BVH : register(t5);

#if !SHADOWS // reflections only
#if STRUCTURED
StructuredBuffer<float4>
#else
ByteAddressBuffer
#endif
VertexBuffer : register(t6);

// #define FLT_MAX 3.402823466e+38f
#define FLT_MAX 1000000000
#define BACKFACE_CULLING 1

struct HitPoint
{
    float3 worldPos;
    float3 normal;
    float2 uv;
    float3 tangent;
    int materialIndex;
};
#endif // !SHADOWS

#define kEpsilon 0.00001f
#define kSelfShadowOffset 0.005f

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
#ifdef BACKFACE_CULLING
		dot(s1, e1) < -kEpsilon ||
#endif // BACKFACE_CULLING
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
			   const float3 rayDirInv
#if !SHADOWS
			   , inout HitPoint hitPoint
#endif // !SHADOWS
)
{
	bool collision = false;
    int dataOffset = 0;
	int offsetToNextNode = 1;
	
	float t = 0;
	float2 bc = 0; // barycentric coords

#if !SHADOWS
	float minDist = FLT_MAX;
	bool hit = false;
#endif // !SHADOWS

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
#else // SHADOWS
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

                hitPoint.worldPos = v0 * (1 - bc.x - bc.y) + v1 * bc.x + v2 * bc.y;
                hitPoint.normal   = n0 * (1 - bc.x - bc.y) + n1 * bc.x + n2 * bc.y;
                hitPoint.uv       = u0 * (1 - bc.x - bc.y) + u1 * bc.x + u2 * bc.y;
                hitPoint.tangent  = t0 * (1 - bc.x - bc.y) + t1 * bc.x + t2 * bc.y;
				hitPoint.materialIndex = element2.w;
            }
#endif // SHADOWS
        }
    }

#if SHADOWS
	return collision;
#else
	return hit; // can we use collision also for reflections?
#endif
}
#include "../RenderToyD3D11/shaders/Fullscreen.hlsl"

cbuffer ObjectCB : register(b0)
{
    float4x4 viewProjInv;
    float3   lightDir;
    float    padding;
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

#define BACKFACE_CULLING 0 // enable for reflections
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

float4 RayTracedShadowsPS(const VertexOut pin) : SV_Target
{
#if NORMALS
    const float4 value = DepthNormalBuffer.Load(uint3(pin.position.xy, 0));
    const float depth = value.w;
    const float normal = value.xyz;
#else
    const float depth = DepthBuffer.Load(uint3(pin.position.xy, 0));
#endif

#if NORMALS
	const float NdotL = dot(normal, -lightDir);

    if (NdotL <= 0)
    {
        // do not raytrace for surfaces that point away from the light
        discard;
    }
#endif

    if (depth == 1)
    {
        // do not raytrace for sky pixels
        discard;
    }

	bool collision = false;
	int offsetToNextNode = 1;
    

    float4 clipPos = float4(2 * pin.uv - 1, depth, 1);
    clipPos.y = -clipPos.y;

    float4 worldPos = mul(viewProjInv, clipPos);
    worldPos.xyz /= worldPos.w;

    float3 rayDir = -lightDir;
    float3 rayDirInv = 1 / rayDir;

#if NORMALS
    // offset to avoid selfshadows
    worldPos.xyz += 5 * normal;
#endif

    // fake normal
    {
        float3 a = ddx(worldPos.xyz);
        float3 b = ddy(worldPos.xyz);
        float3 n = normalize(cross(a, b));

        const float NdotL = dot(n, -lightDir);
        if (NdotL <= 0)
        {
            // do not raytrace for surfaces that point away from the light
            discard;
        }

        worldPos.xyz += 0.005f * n;
    }

    int dataOffset = 0;

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
            collision = RayBoxIntersect(worldPos.xyz, rayDirInv, element0.xyz, element1.xyz);

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

            float t = 0;
            float2 barycentricCoords = 0;

            // check for intersection with leaf triangle
            collision = RayTriIntersect(worldPos.xyz, rayDir, element0.xyz, element1.xyz, element2.xyz, t, barycentricCoords);

            if (collision)
            {
                // break;
                return float4(0,0,0,0.25f);
            }
        }
    }

    // return float4(count/10.f,0,0,1);
    
    discard;
    return 0;
}
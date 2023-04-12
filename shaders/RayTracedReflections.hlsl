#define BACKFACE_CULLING 1
#define SHADOWS 0
#include "RayTracedCommon.hlsl"

cbuffer RayTracedReflectionsCB : register(b1)
{
    float3 eyePos;
    float padding;
};

float4 RayTracedReflectionsPS(const VertexOut pin) : SV_Target
{
#if NORMALS
    const float4 value = DepthNormalBuffer.Load(uint3(pin.position.xy, 0));
    const float depth = value.w;
    const float normal = value.xyz;
#else
    const float depth = DepthBuffer.Load(uint3(pin.position.xy, 0));
#endif

    if (depth == 1)
    {
        // do not raytrace sky pixels
        discard;
    }
    
    float3 worldPos = GetWorldPos(pin.uv, depth);

#if !NORMALS
    // fake normal
    float3 a = ddx(worldPos);
    float3 b = ddy(worldPos);
    float3 n = normalize(cross(a, b));
#endif

    // offset to avoid selfshadows
    worldPos += 0.001f * n;

    // const float3 rayDir = reflect(worldPos - eyePos, float3(0,1,0));
    const float3 rayDir = reflect(worldPos - eyePos, n);
    const float3 rayDirInv = 1 / rayDir;
    
    // float t = 0;
    // float2 barycentricCoords = 0;
    int index = -1;
    float3 normal = 0;

    // if (!RayTraced(worldPos, rayDir, rayDirInv, t, barycentricCoords))
    if (!RayTraced(worldPos, rayDir, rayDirInv, index, normal))
    {
        discard;
    }

    MaterialData material = gMaterialBuffer[index];
    
	float3 toEye = eyePos - worldPos; // should use world pos of reflected triangle?
	const float distToEye = length(toEye);
	toEye /= distToEye;
    
	float4 diffuse = material.diffuse;
    
#if ALPHA_TEST
	clip(diffuse.a - 0.1f);
#endif // ALPHA_TEST

	const float4 ambient = gAmbientLight * diffuse;

	const float shininess = 1.0f - material.roughness;

	const Material mat = { diffuse, material.fresnel, shininess };
    
	const float3 shadow = 1.0f;
    
	const float4 direct = ComputeLighting(gLights, mat, worldPos /* dontcare */, normal, toEye, shadow);
	
	float4 result = ambient + direct;

    // specular reflections
	{
		const float3 r = reflect(-toEye, normal);
		// const float4 reflection = gCubeMap.Sample(gSamplerLinearWrap, r);
		const float3 fresnel = SchlickFresnel(material.fresnel, normal, r);
		// result.rgb += shininess * fresnel * reflection.rgb;
		result.rgb += shininess * fresnel;
	}
    
	result.a = diffuse.a;

    // return float4(0,1,0,0.25f);
    // return float4(material.diffuse.rgb, 0.25f);
    return float4(result.rgb, result.a * 0.25f);
}
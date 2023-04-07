#define LIGHT_MAX_COUNT 16

#ifndef LIGHT_DIR_COUNT
    #define LIGHT_DIR_COUNT 3
#endif

#ifndef LIGHT_POINT_COUNT
    #define LIGHT_POINT_COUNT 0
#endif

#ifndef LIGHT_SPOT_COUNT
    #define LIGHT_SPOT_COUNT 0
#endif

struct Light
{
    float3 strength;
    float falloffStart;
    float3 direction;
    float falloffEnd;
    float3 position;
    float spotPower;
};

struct Material
{
    float4 diffuse;
    float3 fresnel;
    float shininess;
};

float LightAttenuation(const float x, const float a, const float b)
{
    return saturate((b - x) / (b - a));
}

float3 SchlickFresnel(const float3 R0,
                      const float3 normal,
                      const float3 light)
{
    const float angle = saturate(dot(normal, light));

    const float f0 = 1.0f - angle;
    const float3 r = R0 + (1.0f - R0) * (f0*f0*f0*f0*f0);

    return r;
}

float3 BlinnPhong(const float3 strength,
                  const float3 direction,
                  const float3 normal,
                  const float3 toEye,
                  const Material material)
{
    const float m = material.shininess * 256.0f;
    const float3 halfVec = normalize(toEye + direction);

    const float roughness = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
    const float3 fresnel = SchlickFresnel(material.fresnel, halfVec, direction);

    float3 albedo = fresnel * roughness;
    albedo = albedo / (albedo + 1.0f);

    return (material.diffuse.rgb + albedo) * strength;
}

float3 ComputeDirectionalLight(const Light light,
                               const Material material,
                               const float3 normal,
                               const float3 toEye)
{
    // the light vector aims opposite the direction the light rays travel
    const float3 direction = -light.direction;

    // scale light down by Lambert's cosine law.
    const float ndotl = max(dot(direction, normal), 0.0f);
    const float3 strength = light.strength * ndotl;

    return BlinnPhong(strength, direction, normal, toEye, material);
}

float4 ComputeLighting(Light lights[LIGHT_MAX_COUNT],
                       Material material,
                       float3 position,
                       float3 normal,
                       float3 toEye,
                       float3 shadow)
{
    float3 result = 0.0f;

    int i = 0;

#if (LIGHT_DIR_COUNT > 0)
    for(i = 0; i < LIGHT_DIR_COUNT; ++i)
    {
        result += shadow[i] * ComputeDirectionalLight(lights[i], material, normal, toEye);
    }
#endif

// #if (LIGHT_POINT_COUNT > 0)
//     for(i = LIGHT_DIR_COUNT; i < LIGHT_DIR_COUNT + LIGHT_POINT_COUNT; ++i)
//     {
//         result += ComputePointLight(gLights[i], mat, pos, normal, toEye);
//     }
// #endif

// #if (LIGHT_SPOT_COUNT > 0)
//     for(i = LIGHT_DIR_COUNT + LIGHT_POINT_COUNT; i < LIGHT_DIR_COUNT + LIGHT_POINT_COUNT + LIGHT_SPOT_COUNT; ++i)
//     {
//         result += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
//     }
// #endif

    return float4(result, 0.0f);
}
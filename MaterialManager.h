#pragma once

// std
#include <string>
#include <unordered_map>
#include <cassert>

// d3d
#include <directxmath.h>
using namespace DirectX;

struct Material
{
    XMFLOAT4 diffuse   = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    XMFLOAT3 fresnel   = XMFLOAT3(0.01f, 0.01f, 0.01f);
    float    roughness = 0.25f;

    //int DiffuseSRVHeapIndex = -1;
    //int NormalSRVHeapIndex = -1;

    //XMFLOAT4X4 transform = MathHelper::Identity4x4();
};

class MaterialManager
{
public:

    static void AddMaterial(const std::string& name,
                            const Material& material)
    {
        assert(!lookup.contains(name));

        materials.push_back(material);
        lookup[name] = materials.size() - 1;
    }

    static Material& GetMaterial(const std::size_t i)
    {
        assert(i < materials.size());

        return materials[i];
    }

    static std::vector<Material>& GetMaterials()
    {
        return materials;
    }

private:

    static std::unordered_map<std::string, std::size_t> lookup;
    static std::vector<Material> materials;
};
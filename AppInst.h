#pragma once

#include "AppBase.h"

// d3d
#include <directxmath.h>

//
#include "BVH.h"
#include "RayTraced.h"

class AppInst : public AppBase
{
public:

    AppInst(HINSTANCE instance);
    ~AppInst();

    bool Init() override;

    void Update(const Timer& timer) override;
    void Draw(const Timer& timer) override;

private:

    BVH mBVH;
    RayTraced mRayTraced;

    XMFLOAT2 mWavesNormalMapOffset0;
    XMFLOAT2 mWavesNormalMapOffset1;

    struct WavesCB
    {
        XMFLOAT4X4 wavesNormalMapTransform0;
        XMFLOAT4X4 wavesNormalMapTransform1;
    };

    static_assert((sizeof(WavesCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

    ComPtr<ID3D11Buffer> mWavesCB;
};
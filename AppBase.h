#pragma once

// windows
#include <wrl.h>
#include <comdef.h>
using Microsoft::WRL::ComPtr;

// d3d
#include <d3d11.h>
#include <directxmath.h>
using namespace DirectX;

// #include "DDSTextureLoader.h"

// std
#include <string>
#include <sstream>

// 
#include "Camera.h"
#include "GeometryGenerator.h"
#include "Lighting.h"
#include "MaterialManager.h"
#include "Timer.h"

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                                      \
{                                                                             \
    HRESULT hr__ = (x);                                                       \
    std::wstring wfn = AnsiToWString(__FILE__);                               \
    if (FAILED(hr__)) { throw AppBase::Exception(hr__, L#x, wfn, __LINE__); } \
}
#endif // ThrowIfFailed

class AppBase
{
public:

    AppBase(HINSTANCE instance);
    AppBase(const AppBase&) = delete;
    AppBase& operator=(const AppBase&) = delete;
    ~AppBase();

    static AppBase* GetApp();
    LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    virtual bool Init();
    int Run();

    virtual void OnResize();
    virtual void Update(const Timer& timer);
    virtual void Draw(const Timer& timer) = 0;

    struct Exception
    {
        const HRESULT result;
        const std::wstring function;
        const std::wstring file;
        const int line;

        Exception(HRESULT result,
                  const std::wstring& function,
                  const std::wstring& file,
                  int line)
            : result(result)
            , function(function)
            , file(file)
            , line(line)
        {}

        std::wstring ToString() const
        {
            const _com_error err(result);
            const std::wstring msg = err.ErrorMessage();

            std::wostringstream woss;
            woss << L"code: 0x" << std::hex << result << '\n';
            woss << L"message:\n" << msg << "\n\n";
            woss << L"function:\n" << function << "\n\n";
            woss << L"file: " << file << '\n';
            woss << L"line: " << std::dec << line << '\n';

            return woss.str();
        }
    };

protected:

    virtual void OnMouseDown(WPARAM state, int x, int y);
    virtual void OnMouseUp(WPARAM state, int x, int y);
    virtual void OnMouseMove(WPARAM state, int x, int y);
    virtual void OnKeyboardEvent(const Timer& timer);

    virtual void UpdateMainPassCB(const Timer& timer);
    virtual void UpdateMaterialsSB(const Timer& timer);

    void NameResource(ID3D11DeviceChild* pDeviceChild, const std::string& name);

    enum class ShaderTarget
    {
        VS,
        PS,
    };

    ComPtr<ID3DBlob> CompileShader(const std::wstring& fileName,
                                   const D3D_SHADER_MACRO* defines,
                                   const std::string& entryPoint,
                                   const ShaderTarget target);

    ComPtr<ID3D11Device> mDevice;
    ComPtr<ID3D11DeviceContext> mContext;

    ComPtr<ID3D11RenderTargetView> mBackBufferRTV;
    ComPtr<ID3D11DepthStencilView> mDepthStencilBufferDSV;

    D3D11_VIEWPORT mViewport;

    Camera mCamera;

    GeometryGenerator mGeometryGenerator;

    ComPtr<ID3D11Buffer> mVertexBuffer;
    ComPtr<ID3D11Buffer> mIndexBuffer;
    DXGI_FORMAT mIndexBufferFormat = DXGI_FORMAT_R16_UINT;
    ComPtr<ID3D11InputLayout> mInputLayout;

    UINT mIndexStart;
    UINT mIndexCount;
    UINT mVertexBase;

    D3D11_PRIMITIVE_TOPOLOGY mPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // default shaders
    ComPtr<ID3D11VertexShader> mVertexShader;
    ComPtr<ID3D11PixelShader> mPixelShader;

    struct MainPassCB
    {
        //XMFLOAT4X4 view = MathHelper::Identity4x4();
        //XMFLOAT4X4 ViewInverse = MathHelper::Identity4x4();
        //XMFLOAT4X4 proj = MathHelper::Identity4x4();
        //XMFLOAT4X4 ProjInverse = MathHelper::Identity4x4();
        XMFLOAT4X4 viewProj;
//        XMFLOAT4X4 ViewProjInverse = MathHelper::Identity4x4();
//        XMFLOAT4X4 ShadowTransform = MathHelper::Identity4x4();
        XMFLOAT3 eyePosition = { 0.0f, 0.0f, 0.0f };
        float padding1;
//        XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
//        XMFLOAT2 RenderTargetSizeInverse = { 0.0f, 0.0f };
//        float NearPlane = 0.0f;
//        float FarPlane = 0.0f;
//        float DeltaTime = 0.0f;
//        float TotalTime = 0.0f;
//
        XMFLOAT4 ambientLight = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
//
//#if IS_FOG_ENABLED
//        XMFLOAT4 FogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
//        float FogStart = 5.0f;
//        float FogRange = 150.0f;
//        XMFLOAT2 padding2;
//#endif // IS_FOG_ENABLED
//
        Lighting::Light lights[LIGHT_MAX_COUNT];
    };

    static_assert((sizeof(MainPassCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

    struct ObjectCB
    {
        XMFLOAT4X4 world;
        //XMFLOAT4X4 TexCoordTransform = MathHelper::Identity4x4();
        UINT material = -1;
        XMFLOAT3 padding;
    };

    static_assert((sizeof(ObjectCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

    ComPtr<ID3D11Buffer> mMainPassCB;
    ComPtr<ID3D11Buffer> mObjectCB;

    ComPtr<ID3D11ShaderResourceView> mMaterialsBufferSRV;

private:

    bool InitWindow();
    bool InitDirect3D();

    // void CalculateFrameStats();

    static AppBase* mApp;

    HINSTANCE mInstance = nullptr;
    HWND mWindow = nullptr;

    bool mIsAppPaused = false;
    bool mIsWindowMinimized = false;
    bool mIsWindowMaximized = false;
    bool mIsWindowResizing = false;

    Timer mTimer;

    POINT mLastMousePosition = { 0, 0 };

    std::wstring mWindowName = L"Window Name";
    UINT mWindowWidth = 800;
    UINT mWindowHeight = 600;
    float mWindowAspectRatio = float(mWindowWidth) / float(mWindowHeight);

    std::wstring mGPUName;

    UINT mSwapChainBufferCount = 2;
    DXGI_FORMAT mSwapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    UINT mSwapChainFlags = 0;


    ComPtr<IDXGISwapChain> mSwapChain;




    // default states
    ComPtr<ID3D11RasterizerState> mRasterizerState;
    ComPtr<ID3D11BlendState> mBlendState;
    ComPtr<ID3D11DepthStencilState> mDepthStencilState;

    // default samplers
    // textures

    // materials
    MaterialManager mMaterialManager;
    ComPtr<ID3D11Buffer> mMaterialsSB;
    bool mIsMaterialsBufferDirty = true;

    // lights
    Lighting mLighting;

    // objects

    //std::vector<GeometryGenerator::MeshData> mMeshes;
    GeometryGenerator::MeshData mMesh;
};
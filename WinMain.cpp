#include "AppInst.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, PSTR CMD, int ShowCMD)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    try
    {
        AppInst app(instance);

        if (!app.Init())
        {
            return 0;
        }

        return app.Run();
    }
    catch (AppBase::Exception& exception)
    {
        MessageBox(nullptr, exception.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}
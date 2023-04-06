#pragma once

#include "AppBase.h"

class AppInst : public AppBase
{
public:
    AppInst(HINSTANCE instance);
    ~AppInst();

    bool Init() override;

    void Update(const Timer& timer) override;
    void Draw(const Timer& timer) override;

private:

};
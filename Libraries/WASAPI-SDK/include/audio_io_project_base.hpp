#pragma once

#include <windef.h>

class WASAPIIOHandler {
  public:
    virtual void inputCallback(BYTE* pBuffer, std::size_t availableFrameCnt) noexcept = 0;
    virtual void outputCallback(std::size_t availableFrameCnt, BYTE* pBuffer) noexcept = 0;
    virtual ~WASAPIIOHandler() {}
};


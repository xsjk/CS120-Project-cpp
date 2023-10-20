#pragma once

#include "iohandler.hpp"


class WASAPIIOHandler : public IOHandler<int> {
public:
    virtual void inputCallback(const int* const* inputData, int numInputChannels, int numSamples) noexcept {

    }

    virtual void outputCallback(int* const* outputData, int numOutputChannels, int numSamples) noexcept {

    }

    virtual void inputCallback(BYTE* pBuffer, std::size_t availableFrameCnt) noexcept = 0;
    virtual void outputCallback(std::size_t availableFrameCnt, BYTE* pBuffer) noexcept = 0;
    virtual ~WASAPIIOHandler() {}
};


namespace WASAPI {

    class CallbackHandler {

    public:
        virtual void inputCallback(BYTE* pBuffer, std::size_t availableFrameCnt) noexcept = 0;
        virtual void outputCallback(std::size_t availableFrameCnt, BYTE* pBuffer) noexcept = 0;
        virtual ~CallbackHandler() {}

    };


}
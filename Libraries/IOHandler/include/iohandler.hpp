#pragma once

template <typename T>
class IOHandler {
public:
    virtual void inputCallback(const T* const* inputData, int numInputChannels, int numSamples) noexcept = 0;
    virtual void outputCallback(T* const* outputData, int numOutputChannels, int numSamples) noexcept = 0;
    virtual ~IOHandler() {}
};

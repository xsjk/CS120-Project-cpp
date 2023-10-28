#pragma once

#include "audioiohandler.hpp"
#include <cstring>

namespace WASAPI {

    class DataView : public AudioDataView<short> {
        short *data;

    public:

        DataView(BYTE *pBuffer, size_t bufferSize, double sampleRate) :
            AudioDataView<short>(2, bufferSize, sampleRate),
            data((short *)pBuffer) { }

        FloatView<short> operator()(size_t i, size_t j) noexcept override {
            return FloatView(data[i + j * getNumChannels()]);
        }

        float operator()(size_t i, size_t j) const noexcept override {
            return FloatView(data[i + j * getNumChannels()]);
        }

        void zero() noexcept override {
            std::memset(data, 0, getNumChannels() * getNumSamples() * sizeof(short));
        }

    };

    struct IOHandler : AudioIOHandler<short> {
        virtual void inputCallback(const AudioDataView<short> &inputData) noexcept {
            inputCallback((const DataView &)inputData);
        }
        virtual void outputCallback(AudioDataView<short> &outputData) noexcept {
            inputCallback((DataView &)outputData);
        }
        virtual void inputCallback(const DataView &) noexcept {}
        virtual void outputCallback(DataView &) noexcept {}
    };

}
#pragma once

#include "audioiohandler.hpp"

namespace WASAPI {

    class DataView : public AudioDataView<short> {
        short *data;
    public:
        DataView(BYTE *pBuffer, size_t bufferSize, double frequency) :
            AudioDataView<short>(2, bufferSize, frequency),
            data((short *)pBuffer) { }

        FloatView<short> operator()(size_t i, size_t j) noexcept override {
            return FloatView(data[i + j * getNumChannels()]);
        }

        float operator()(size_t i, size_t j) const noexcept override {
            return FloatView(data[i + j * getNumChannels()]);
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
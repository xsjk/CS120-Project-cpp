#pragma once

#include "audioiohandler.hpp"
#include <iostream>
#include <cstring>


namespace ASIO {

    class DataView : public AudioDataView<int> {
        int *const *data;
    public:
        DataView(const int *const *channelData, int channels, int samples, double sampleRate) :
            AudioDataView<int>(channels, samples, sampleRate),
            data(const_cast<int* const*>(channelData)) { }

        FloatView<int> operator()(size_t i, size_t j) noexcept override {
            return FloatView(data[i][j]);
        }

        float operator()(size_t i, size_t j) const noexcept override {
            return FloatView(data[i][j]);
        }

        void zero() noexcept override {
            for (auto i = 0; i < getNumChannels(); i++)
                std::memset(data[i], 0, getNumSamples() * sizeof(int));
        }

    };

    struct IOHandler : AudioIOHandler<int> {
        virtual void inputCallback(const AudioDataView<int> &inputData) noexcept {
            inputCallback(static_cast<const DataView &>(inputData));
        }
        virtual void outputCallback(AudioDataView<int> &outputData) noexcept {
            inputCallback((DataView &)outputData);
        }
        virtual void inputCallback(const DataView &) noexcept { }
        virtual void outputCallback(DataView &) noexcept { }
    };

}

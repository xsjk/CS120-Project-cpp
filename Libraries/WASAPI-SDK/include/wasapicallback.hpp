#pragma once

#include "audioiohandler.hpp"
#include <cstring>

namespace WASAPI {
    
    class RawDataView {
        short *data;
        size_t numChannels;
        size_t numSamples;
        double sampleRate;
    public:
    
        auto size() const noexcept { return numSamples * numChannels; }
        auto getNumChannels() const noexcept { return numChannels; }
        auto getNumSamples() const noexcept { return numSamples; }
        auto getSampleRate() const noexcept { return sampleRate; }
        
        RawDataView(BYTE *pBuffer, size_t bufferSize, double sampleRate) :
            data((short *)pBuffer), numChannels(2), numSamples(bufferSize), sampleRate(sampleRate) {}

        short& operator()(size_t i, size_t j) noexcept {
            return data[i + j * getNumChannels()];
        }

        short operator()(size_t i, size_t j) const noexcept {
            return data[i + j * getNumChannels()];
        }

        void zero() noexcept {
            std::memset(data, 0, getNumChannels() * getNumSamples() * sizeof(short));
        }

    };

    template<typename V>
    class DataView : public AudioDataProxy<short, V> {
        short *data;

    public:

        using AudioDataProxy<short, V>::getNumChannels;
        using AudioDataProxy<short, V>::getNumSamples;
        using AudioDataProxy<short, V>::getSampleRate;
        using AudioDataProxy<short, V>::size;

        DataView(BYTE *pBuffer, size_t bufferSize, double sampleRate) :
            AudioDataProxy<short, V>(2, bufferSize, sampleRate),
            data((short *)pBuffer) { }

        ArithmeticProxy<short> operator()(size_t i, size_t j) noexcept override {
            return ArithmeticProxy(data[i + j * getNumChannels()]);
        }

        V operator()(size_t i, size_t j) const noexcept override {
            return ArithmeticProxy(data[i + j * getNumChannels()]);
        }

        void zero() noexcept override {
            std::memset(data, 0, getNumChannels() * getNumSamples() * sizeof(short));
        }

    };

    template<typename V>
    struct IOHandler : AudioIOHandler<short, V> {
        virtual void inputCallback(const AudioDataProxy<short, V> &inputData) noexcept {
            inputCallback(reinterpret_cast<const DataView<float> &>(inputData));
        }
        virtual void inputCallback(AudioDataProxy<short, V> && d) noexcept { inputCallback(d); }
        virtual void outputCallback(AudioDataProxy<short, V> &outputData) noexcept {
            inputCallback(reinterpret_cast<DataView<float> &>(outputData));
        }
        virtual void inputCallback(const DataView<float> &) noexcept {}
        virtual void inputCallback(DataView<float> && d) noexcept { inputCallback(d); }
        virtual void outputCallback(DataView<float> &) noexcept {}
    };

}
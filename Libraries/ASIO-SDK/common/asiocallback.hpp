#pragma once

#include "audioiohandler.hpp"
#include <iostream>
#include <cstring>


namespace ASIO {

    class RawDataView {
        int *const *data;
        size_t numChannels;
        size_t numSamples;
        double sampleRate;
    public:
    
        auto size() const noexcept { return numSamples * numChannels; }
        auto getNumChannels() const noexcept { return numChannels; }
        auto getNumSamples() const noexcept { return numSamples; }
        auto getSampleRate() const noexcept { return sampleRate; }

        RawDataView(const int *const *channelData, int channels, int samples, double sampleRate) :
            data(const_cast<int* const*>(channelData)), numChannels(channels), numSamples(samples), sampleRate(sampleRate) {}

        int& operator()(size_t i, size_t j) noexcept {
            return data[i][j];
        }

        int operator()(size_t i, size_t j) const noexcept {
            return data[i][j];
        }

        void zero() noexcept {
            for (auto i = 0; i < getNumChannels(); i++)
                std::memset(data[i], 0, getNumSamples() * sizeof(int));
        }

    };

    template<typename V>
    class DataView : public AudioDataProxy<int, V> {
        int *const *data;
    public:
    
        using AudioDataProxy<int, V>::getNumChannels;
        using AudioDataProxy<int, V>::getNumSamples;
        using AudioDataProxy<int, V>::getSampleRate;
        using AudioDataProxy<int, V>::size;

        DataView(const int *const *channelData, int channels, int samples, double sampleRate) :
            AudioDataProxy<int, V>(channels, samples, sampleRate),
            data(const_cast<int* const*>(channelData)) { }

        ArithmeticProxy<int, V> operator()(size_t i, size_t j) noexcept override {
            return ArithmeticProxy<int, V>(data[i][j]);
        }

        V operator()(size_t i, size_t j) const noexcept override {
            return ArithmeticProxy<int, V>(data[i][j]);
        }

        void zero() noexcept override {
            for (auto i = 0; i < getNumChannels(); i++)
                std::memset(data[i], 0, getNumSamples() * sizeof(int));
        }

    };

    template<typename V>
    struct ASIO_API IOHandler : AudioIOHandler<int, V> {
        virtual void inputCallback(const AudioDataProxy<int, V> &inputData) noexcept {
            inputCallback(reinterpret_cast<const DataView<V> &>(inputData));
        }
        virtual void inputCallback(AudioDataProxy<int, V> &&inputData) noexcept { inputCallback(inputData); }
        virtual void outputCallback(AudioDataProxy<int, V> &outputData) noexcept {
            inputCallback(reinterpret_cast<DataView<V> &>(outputData));
        }
        virtual void inputCallback(const DataView<V> &) noexcept { }
        virtual void inputCallback(DataView<V> && inputData) noexcept { inputCallback(inputData); }
        virtual void outputCallback(DataView<V> &) noexcept { }
    };

}

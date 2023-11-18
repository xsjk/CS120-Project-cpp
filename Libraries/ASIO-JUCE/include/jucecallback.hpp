#pragma once

#include <cstring>
#include <memory>
#include "utils.hpp"
#include "audioiohandler.hpp"
#include <JuceHeader.h>

namespace JUCE {
    
    class RawDataView {
        float* const* data;
        size_t numChannels;
        size_t numSamples;
        double sampleRate;
    public:
    
        auto size() const noexcept { return numSamples * numChannels; }
        auto getNumChannels() const noexcept { return numChannels; }
        auto getNumSamples() const noexcept { return numSamples; }
        auto getSampleRate() const noexcept { return sampleRate; }
        
        RawDataView(const float* const* data, size_t numChannels, size_t numSamples, double sampleRate) :
            data(const_cast<float* const*>(data)), numChannels(numChannels), numSamples(numSamples), sampleRate(sampleRate) { }
        
        float& operator()(size_t i, size_t j) noexcept {
            return data[i][j];
        }

        float operator()(size_t i, size_t j) const noexcept {
            return data[i][j];
        }

        void zero() noexcept {
            for (auto i = 0; i < getNumChannels(); i++)
                std::memset(data[i], 0, getNumSamples() * sizeof(float));
        }

    };

    template<typename V>
    class DataView : public AudioDataProxy<float, V> {
        float *const *data;
    public:
    
        using AudioDataProxy<float, V>::getNumChannels;
        using AudioDataProxy<float, V>::getNumSamples;
        using AudioDataProxy<float, V>::getSampleRate;
        using AudioDataProxy<float, V>::size;

        DataView(const float *const *channelData, float channels, float samples, double sampleRate) :
            AudioDataProxy<float, V>(channels, samples, sampleRate),
            data(const_cast<float* const*>(channelData)) { }

        ArithmeticProxy<float, V> operator()(size_t i, size_t j) noexcept override {
            return ArithmeticProxy<float, V>(data[i][j]);
        }

        V operator()(size_t i, size_t j) const noexcept override {
            return ArithmeticProxy<float, V>(data[i][j]);
        }

        void zero() noexcept override {
            for (auto i = 0; i < getNumChannels(); i++)
                std::memset(data[i], 0, getNumSamples() * sizeof(float));
        }

    };

    template<typename V>
    struct IOHandler : AudioIOHandler<float, V> {
        virtual void inputCallback(const AudioDataProxy<float, V> &inputData) noexcept {
            inputCallback(reinterpret_cast<const DataView<V> &>(inputData));
        }
        virtual void inputCallback(AudioDataProxy<float, V> &&inputData) noexcept { inputCallback(inputData); }
        virtual void outputCallback(AudioDataProxy<float, V> &outputData) noexcept {
            inputCallback(reinterpret_cast<DataView<V> &>(outputData));
        }
        virtual void inputCallback(const DataView<V> &) noexcept { }
        virtual void inputCallback(DataView<V> && inputData) noexcept { inputCallback(inputData); }
        virtual void outputCallback(DataView<V> &) noexcept { }
    };


    class IOCallback : public juce::AudioIODeviceCallback {

        std::shared_ptr<IOHandler<float>> callback;
        double sample_rate;

    public:
        IOCallback(const std::shared_ptr<IOHandler<float>>& callback, double sample_rate): callback(callback), sample_rate(sample_rate) { }

        void audioDeviceAboutToStart(juce::AudioIODevice*) override {}

        void audioDeviceStopped() override {}

        void audioDeviceIOCallbackWithContext(
            const float* const* inputChannelData,
            int numInputChannels,
            float* const* outputChannelData,
            int numOutputChannels,
            int numSamples,
            const juce::AudioIODeviceCallbackContext& context
        ) override {
            auto input = DataView<float>(inputChannelData, numInputChannels, numSamples, sample_rate);
            auto output = DataView<float>(outputChannelData, numOutputChannels, numSamples, sample_rate);
            callback->inputCallback(input);
            callback->outputCallback(output);
        }

    };

}

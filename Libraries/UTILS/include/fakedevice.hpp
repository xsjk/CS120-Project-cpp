#pragma once

#include <vector>
#include <fstream>
#include <span>
#include <memory>
#include <thread>
#include <iostream>

#include "audioiohandler.hpp"


namespace FAKE {
    
    // fake dataview with one channel
    template<typename V>
    class DataView : public AudioDataProxy<float, V> {

        std::span<V> data;

    public:
        
        using AudioDataProxy<float, V>::getNumChannels;
        using AudioDataProxy<float, V>::getNumSamples;
        using AudioDataProxy<float, V>::getSampleRate;
        using AudioDataProxy<float, V>::size;

        DataView(std::span<V>&& data, double sampleRate) : 
            AudioDataProxy<float, V>(1, data.size(), sampleRate),
            data(std::move(data)) { }

        ArithmeticProxy<float, V> operator()(size_t i, size_t j) noexcept override {
            return data[j];
        }
        V operator()(size_t i, size_t j) const noexcept override {
            return data[j];
        }
        auto operator[](size_t) noexcept { return data; }
        const auto operator[](size_t) const noexcept { return data; }


    };


    template<typename V>
    struct IOHandler : AudioIOHandler<float, V> {
        virtual void inputCallback(const DataView<float> &) noexcept {}
        virtual void inputCallback(DataView<float> &&d) noexcept { inputCallback(d); }
        virtual void outputCallback(DataView<float> &) noexcept {}
        virtual void inputCallback(const AudioDataProxy<float, V> &inputData) noexcept {
            inputCallback(reinterpret_cast<const DataView<float> &>(inputData));
        }
        virtual void inputCallback(AudioDataProxy<float, V> &&inputData) noexcept { inputCallback(inputData); }
        virtual void outputCallback(AudioDataProxy<float, V> &outputData) noexcept {
            outputCallback(reinterpret_cast<DataView<float> &>(outputData));
        }
    };


    class Device : public std::enable_shared_from_this<Device> {
        std::vector<float> fakeInput;
        size_t buffer_size;
        double sampleRate;
        std::jthread thread;
    public:
        Device(std::string file, size_t buffer_size) : fakeInput{[&] {
                std::ifstream input { file };
                std::vector<float> fakeInput;
                float v;
                while (input >> v)
                    fakeInput.push_back(v);
                return fakeInput;
            }()}, buffer_size(buffer_size)
        {
            std::ifstream input { file };
            float v;
            while (input >> v)
                fakeInput.push_back(v);
            
            auto offset = fakeInput.size() % buffer_size;
            if (offset != 0)
                for (int i = 0; i < buffer_size - offset; i++)
                    fakeInput.push_back(0);
        }

        
        void open(int numInputChannels = 1, int numOutputChannels = 1, double sampleRate = 44100) {
            this->sampleRate = sampleRate;
        }
        
        void start(std::shared_ptr<IOHandler<float>> handler) {
            auto self = shared_from_this();
            thread = std::jthread([self, handler]( std::stop_token stoken ) {

                std::vector<float> fakeOutput(self->fakeInput.size());
                auto time = std::chrono::steady_clock::now();

                for (int i = 0; i < self->fakeInput.size(); i += self->buffer_size) {
                    std::cout << "fake device: " << i << std::endl;
                    if (stoken.stop_requested()) {
                        std::cout << "fake device: stop requested" << std::endl;
                        return;
                    }
                    auto input = DataView<float> { std::span<float> { self->fakeInput.data() + i, self->buffer_size }, self->sampleRate };
                    handler->inputCallback(std::move(input));
                    auto output = DataView<float> { std::span<float> { fakeOutput } , self->sampleRate };
                    handler->outputCallback(output);
                    std::this_thread::sleep_until(time += std::chrono::milliseconds(int(1000 * self->buffer_size / self->sampleRate)));
                }
            });
        }

        void stop() {
            thread = {};
        }

    };
}

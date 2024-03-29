#include <iostream>
#include <cstring>
#include <chrono>
#include <cmath>
#include <memory>
#include <utility>
#include <fstream>
#include <vector>
#include <mutex>
#include <algorithm>
#include <queue>
#include <limits>
#include <valarray>
#include <thread>
#include <numbers>

#include "wasapidevice.hpp"

using namespace WASAPI;

class SineWave : public IOHandler<float> {
    float phase = 0;
public:

    void outputCallback(DataView<float> &p) noexcept override {
        for (auto i = 0; i < p.getNumSamples(); i++)
            p[0][i] = p[1][i] = std::sin(phase += 2 * std::numbers::pi * 440 / 48000);
    }

};


class ChirpWave : public IOHandler<float> {

    struct Chirp {
        float freq_start;
        float freq_end;
        float duration;
        float phase = 0;

        float operator()(float t) {
            return std::sin(2 * std::numbers::pi * ((freq_end - freq_start) / 2 * t / duration  + freq_start) * t + phase);
        }

    } chirp {
        .freq_start = 12000,
        .freq_end = 12000,
        .duration = 48000 / 48000.,
    };

    float t = 0;
    float t2 = 1;
    float duration = 1024 / 48000.;

    std::ofstream file;

public:
    ChirpWave() : file("test.txt") {}
    
    void outputCallback(DataView<float> &p) noexcept override {
        std::cout << "t = " << t << std::endl;
        for (auto i = 0; i < p.getNumSamples(); i++, t += 1 / p.getSampleRate()) {
            p[0][i] = std::sin(2 * std::numbers::pi * 440 * t) / 4;
            // p[1][i] = 0;
            if (t > t2 && t < t2 + duration) {
                p[0][i] += chirp(t - t2) / 2;
            } 
        }
    }

    void inputCallback(const DataView<float> &inputData) noexcept override {
        auto availableFrameCnt = inputData.getNumSamples();
        for (auto i = 0; i < availableFrameCnt; i++) {
            file << inputData[0][i] << std::endl;
        }
    }


};  

class Recorder : public IOHandler<float> {
    std::ofstream file;
    std::queue<float> data;

public:

    Recorder() {
        file.open("test.txt");
    }

    void inputCallback(const DataView<float> &inputData) noexcept override {
        for (auto i = 0; i < inputData.getNumSamples(); i++) {
            float v = inputData[0][i];
            file << v << std::endl;
            data.push(v);
        }
    }

    void outputCallback(DataView<float> &outputData) noexcept override {
        for (auto i = 0; i < outputData.getNumSamples(); i++) {
            if (data.empty()) {
                std::cout << "No more data to play" << std::endl;
                break;
            }
            outputData[0][i] = data.front();
            outputData[1][i] = 0;
            data.pop();
        }
    }

};

int main() {
    auto p = std::make_shared<ChirpWave>();
    auto device = std::make_shared<Device>();
    device->open();
    device->start(p);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;

}

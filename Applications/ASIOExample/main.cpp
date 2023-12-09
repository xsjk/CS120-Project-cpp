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

#include "asiodevice.h"

using namespace ASIO;

class SineWave : public IOHandler<float> {
    std::ofstream file;
    float phase = 0;
public:

    void outputCallback(DataView<float> &p) noexcept override {
        for (auto i = 0; i < p.getNumSamples(); i++) {
            phase += 2 * std::numbers::pi * 440 / p.getSampleRate();
            p(0, i) = std::sin(phase);
            p(1, i) = std::sin(phase);
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
        auto availableFrameCnt = inputData.getNumSamples();
        for (auto i = 0; i < availableFrameCnt; i++) {
            float v = inputData(0, i);
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
            outputData(0, i) = data.front();
            outputData(1, i) = 0;
            data.pop();
        }
    }

};

int main() {
    auto p = std::make_shared<Recorder>();
    Device device;
    device.open();
    device.start(p);
    std::this_thread::sleep_for(std::chrono::seconds(3));
    return 0;

}

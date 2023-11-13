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

// #include "asiodevice.h"
// using namespace ASIO;

#include "wasapidevice.hpp"
using namespace WASAPI;


class SineWave : public IOHandler {
    float phase1 = 0, phase2 = 0;
public:

    void outputCallback(DataView &p) noexcept override {
        for (auto i = 0; i < p.getNumSamples(); i++) {
            phase1 += 2 * std::numbers::pi * 1000 / p.getSampleRate();
            phase2 += 2 * std::numbers::pi * 10000 / p.getSampleRate();
            phase1 = std::fmod(phase1, 2 * std::numbers::pi);
            phase2 = std::fmod(phase2, 2 * std::numbers::pi);
            p[0][i] = p[1][i] = (std::sin(phase1) + std::sin(phase2)) / 2;
        }
    }

};

int main() {
    auto p = std::make_shared<SineWave>();
    auto device = std::make_shared<Device>();
    device->open();
    device->start(p);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    device->stop();
    return 0;

}
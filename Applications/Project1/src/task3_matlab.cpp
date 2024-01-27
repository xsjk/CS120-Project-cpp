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
#include <mutex>
#include <optional>
#include <functional>
#include <iterator>
#include <random>

#include "signal.hpp"

#include "asiodevice.h"
using namespace ASIO;
// #include "wasapidevice.hpp"
// using namespace WASAPI;

std::vector<float> preamble, carrier;
std::vector<float> sSignal, rSignal;

class Senceiver : public IOHandler<float> {
    void outputCallback(DataView<float> &buffer) noexcept override {
        static auto sampleIndex = 0;
        for (auto i = 0; i < buffer.getNumSamples(); i++, sampleIndex++) {
            buffer(0, i) = sampleIndex < sSignal.size() ? sSignal[sampleIndex] : 0;
            buffer(1, i) = sampleIndex < sSignal.size() ? sSignal[sampleIndex] : 0;
        }
    }
    void inputCallback(const DataView<float> &buffer) noexcept override {
        static auto sampleIndex = 0;
        for (auto i = 0; i < buffer.getNumSamples(); i++, sampleIndex++) {
            rSignal.emplace_back(buffer(0, i));
        }
    }
};

int main() {

    std::ofstream debugFile {"debug.txt"};
    std::ifstream preambleFile {"preamble.txt"}, sDataFile {"sData.txt"};
    std::ofstream sSignalFile {"sSignal.txt"}, rSignalFile {"rSignal.txt"}, rDataFile {"rData.txt"};
    std::vector<bool> sData, rData;
    const int packetBits = 100, interSize = 50, carrierSize = 40;
    preamble.reserve(400);
    carrier.reserve(carrierSize);
    sSignal.reserve(600000);
    rSignal.reserve(600000);

    float preambleSample;
    while (preambleFile >> preambleSample) {
        preamble.emplace_back(preambleSample);
    }

    for (auto i = 0; i < carrierSize; i++) {
        carrier.emplace_back(std::sinf(2*std::numbers::pi*2345*i/(carrierSize-1)));
    }

    bool dataBit;
    while (sDataFile >> dataBit) {
        sData.emplace_back(dataBit);
    }

// ------------------ generate the signal to send -----------------
    for (auto i = 0; i < sData.size(); i+=packetBits) {
        for (auto j = 0; j < interSize; j++) {
            sSignal.emplace_back(0.f);
        }
        for (auto j = 0; j < preamble.size(); j++) {
            sSignal.emplace_back(preamble[j]);
        }
        for (auto j = 0; j < packetBits; j++) {
            for (auto k = 0; k < carrierSize; k++) {
                sSignal.emplace_back(carrier[k] * (sData[i+j] == 0 ? 1 : -1));
            }
        }
        for (auto j = 0; j < interSize; j++) {
            sSignal.emplace_back(0.f);
        }
    }
// ----------------------------------------------------------------

    auto senceiver = std::make_shared<Senceiver>();
    auto device = std::make_shared<Device>();
    device->open();

    std::cout << std::endl;
    std::cout << "Generating sSignal ..." << std::endl;

    for (auto const &d : sSignal) {
        sSignalFile << d << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    device->start(senceiver);
    std::this_thread::sleep_for(std::chrono::seconds(15));
    device->stop();

    for (auto const &d : rSignal) {
        rSignalFile << d << std::endl;
    }

// ------------------ extract data from signl -----------------

    std::cout << std::endl;
    std::cout << "Extracting ..." << std::endl;

    std::deque<float> buffer;
    int state = 0; // 0 for preamble detection and 1 for data extraction
    float lastBigSumI = 0;
    for (auto i = 0; i < rSignal.size(); i++) {
        if (state == 0) {
            buffer.emplace_back(rSignal[i]);
            if (buffer.size() < preamble.size()) {
                continue;
            }
            float sum = 0;
            for (auto j = 0; j < preamble.size(); j++) {
                sum += buffer[j] * preamble[j];
            }
            buffer.pop_front();
            // TODO: how to extract the local maxima
            if (sum > 0.3 && i - lastBigSumI > preamble.size()) {
                // std::cout << i+1 << '\t' << sum << std::endl;
                lastBigSumI = i;
                buffer.clear();
                state = 1;
            }
        }
        else if (state == 1) {
            buffer.emplace_back(rSignal[i]);
            if (buffer.size() < packetBits * carrierSize)
                continue;
            for (auto sampleIndex = 0; sampleIndex < packetBits * carrierSize; ) {
                float sum = 0;
                for (auto j = 0; j < carrierSize; sampleIndex++, j++) {
                    sum += buffer[sampleIndex] * carrier[j];
                }
                rData.emplace_back(sum > 0 ? 0 : 1);
            }
            buffer.clear();
            state = 0;
        }
    }
// ------------------------------------------------------------

    std::cout << std::endl;
    std::cout << "Generating rData ..." << std::endl;

    for (auto const &d : rData) {
        rDataFile << d << std::endl;
    }
    std::cout << std::endl;

    return 0;

}

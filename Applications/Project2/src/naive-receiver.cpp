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
#include <utils.hpp>
#include <8b10b.h>
#include <bitset>

#include "signal.hpp"

#include "asiodevice.h"
using namespace ASIO;

std::vector<float> preamble, carrier;
std::vector<float> rSignal;

class Receiver : public IOHandler<float> {
    void outputCallback(DataView<float> &buffer) noexcept override {
        for (auto i = 0; i < buffer.getNumSamples(); i++) {
            buffer(0, i) = 0;
            buffer(1, i) = 0;
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
    std::ifstream preambleFile {"preamble.txt"};
    std::ofstream rSignalFile {"rSignal.txt"}, rDataFile {"rData.txt"};
    BitsContainer rData, rDataEncoded;
    const int packetBits = 500, carrierSize = 2;
    preamble.reserve(100);
    carrier.reserve(carrierSize);
    rSignal.reserve(200000);
    rDataEncoded.reserve(100000);

    float preambleSample;
    while (preambleFile >> preambleSample) {
        preamble.emplace_back(preambleSample);
    }

    for (auto i = 0; i < carrierSize; i++) {
        carrier.emplace_back(1);
    }

// ----------------------------------------------------------------

    auto receiver = std::make_shared<Receiver>();
    auto device = std::make_shared<Device>();
    device->open();

    std::cout << std::endl;
    std::cout << "Starting device ..." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(1));
    device->start(receiver);
    std::this_thread::sleep_for(std::chrono::seconds(10));
    device->stop();

    for (auto const &d : rSignal) {
        rSignalFile << d << std::endl;
    }

// ------------------ extract data from signl -----------------
    float threshold = 3;

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
            if (sum > threshold && i - lastBigSumI > preamble.size()) {
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
                rDataEncoded.emplace_back(sum > 0 ? 0 : 1);
            }
            buffer.clear();
            state = 0;
        }
    }

// --------------------- apply 8b10b decoding ---------------------
    for (auto i = 0; i < rDataEncoded.size() / 10; i++) {
       rData.push<8>(B8B10::decode(rDataEncoded.get<10>(i)));
    }
// ----------------------------------------------------------------

    std::cout << std::endl;
    std::cout << "Generating rData ..." << std::endl;

    for (auto const &d : rData) {
        rDataFile << d << std::endl;
    }
    std::cout << std::endl;

    std::ofstream outputDataFile("OUTPUT.bin", std::ios::binary);
    char byteOut;
    for (auto const &byteOut : rData.as_span<char>()) {
        outputDataFile.write(&byteOut, sizeof(byteOut));
    }

    return 0;
}

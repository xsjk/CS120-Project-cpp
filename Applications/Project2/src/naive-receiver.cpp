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
#include "CRC.hpp"

#include "asiodevice.h"
using namespace ASIO;


class Receiver : public IOHandler<float> {

    std::vector<float> preamble, carrier;
    std::vector<float> rSignal;

    const float threshold = 3;
    const int packetBits = 500;
    const int crcBits = 8;
    const int carrierSize = 2;
    const int interSize = 10;

    BitsContainer rData;
    BitsContainer rDataEncoded;
    
public:

    Receiver() {

        std::ofstream debugFile {"debug.txt"};
        std::ifstream preambleFile {"preamble.txt"};
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

    }


    int ack_to_send = 0;

    void outputCallback(DataView<float> &output) noexcept override {
        // send ack if input succeeds
        auto i = 0;
        while (ack_to_send) {
            for (; i < preamble.size(); i++)
                output(0, i) = preamble[preamble.size() - i - 1];
            for (; i < interSize; i++)
                output(0, i) = 0;
        }
        for (; i < output.getNumSamples(); i++)
            output(0, i) = 0;
    }

    std::deque<float> buffer;
    CRC8<0x7> crc_checker;

    int state = 0; // 0 for preamble detection and 1 for data extraction

    float lastBigSumI = 0;

    void inputCallback(const DataView<float> &input) noexcept override {
        static auto sampleIndex = 0;
        for (auto i = 0; i < input.getNumSamples(); i++, sampleIndex++) {
            float v = input(0, i);

            if (state == 0) {

                buffer.emplace_back(v);
                if (buffer.size() < preamble.size())
                    continue;

                float sum = 0;
                for (auto j = 0; j < preamble.size(); j++)
                    sum += buffer[j] * preamble[j];
                buffer.pop_front();

                if (sum > threshold && i - lastBigSumI > preamble.size()) {
                    lastBigSumI = i;
                    buffer.clear();
                    state = 1;
                }

            } else {

                buffer.emplace_back(v);
                if (buffer.size() < (packetBits + crcBits) * carrierSize)
                    continue;
                for (auto sampleIndex = 0; sampleIndex < (packetBits + crcBits) * carrierSize; ) {
                    float sum = 0;
                    for (auto j = 0; j < (packetBits + crcBits); sampleIndex++, j++)
                        sum += buffer[sampleIndex] * carrier[j];
                    rDataEncoded.emplace_back(sum > 0 ? 0 : 1);
                }
                // check CRC
                if (crc_checker.check(rDataEncoded.as_span<uint8_t>()) == true)
                    ack_to_send ++;
                buffer.clear();
                state = 0;
            }
        }
    }


    ~Receiver() {

        std::ofstream rSignalFile {"rSignal.txt"};
        for (auto const &d : rSignal) {
            rSignalFile << d << std::endl;
        }

        std::cout << "Extracting ..." << std::endl;

        // --------------------- apply 8b10b decoding ---------------------
        for (auto i = 0; i < rDataEncoded.size() / 10; i++)
            rData.push<8>(B8B10::decode(rDataEncoded.get<10>(i)));

        // ----------------------------------------------------------------

        std::ofstream outputDataFile("OUTPUT.bin", std::ios::binary);
        char byteOut;
        for (auto const &byteOut : rData.as_span<char>())
            outputDataFile.write(&byteOut, sizeof(byteOut));

    }
};


int main() {

    auto receiver = std::make_shared<Receiver>();
    auto device = std::make_shared<Device>();
    device->open();

    std::cout << std::endl;
    std::cout << "Starting device ..." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(1));
    device->start(receiver);
    std::this_thread::sleep_for(std::chrono::seconds(20));
    device->stop();


    return 0;
}

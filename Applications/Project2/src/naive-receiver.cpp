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

#include "fakedevice.hpp"
using namespace FAKE;
using namespace utils;

class Receiver : public IOHandler<float> {

    std::vector<float> preamble, carrier;
    std::vector<float> rSignal;

    const float threshold = 3;
    const int bytesPerCRCCheck = 39;
    const int packetBits = (bytesPerCRCCheck + 1) * 10;
    const int carrierSize = 2;
    const int interSize = 10;

    BitsContainer rData;
    BitsContainer rDataEncoded;
    ByteContainer rDataDecoded;
    
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
            std::cout << "Send ACK" << std::endl;
            for (; i < preamble.size(); i++)
                output(0, i) = preamble[preamble.size() - i - 1];
            for (; i < interSize; i++)
                output(0, i) = 0;
            ack_to_send --;
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
                if (buffer.size() < packetBits * carrierSize)
                    continue;
                for (auto sampleIndex = 0; sampleIndex < packetBits * carrierSize; ) {
                    float sum = 0;
                    for (auto j = 0; j < carrierSize; sampleIndex++, j++)
                        sum += buffer[sampleIndex] * carrier[j];
                    rDataEncoded.emplace_back(sum > 0 ? 0 : 1);
                }

                if (rDataEncoded.size() != packetBits) {
                    std::cout << "ERROR: " << rDataEncoded.size() << std::endl;
                }

                // to 8b10b
                for (auto i = 0; i < rDataEncoded.size() / 10 - 1; i++)
                    rDataDecoded.push_back((uint8_t)B8B10::decode(rDataEncoded.get<10>(i)).to_ulong());

                std::cout << rDataEncoded.size() << ' ' << rDataDecoded.size() << std::endl;
                // check CRC
                if (crc_checker.check(
                    std::span(rDataDecoded.end() - bytesPerCRCCheck, rDataDecoded.end()),
                    (uint8_t)B8B10::decode(rDataEncoded.get<10>(bytesPerCRCCheck)).to_ulong())
                ) {
                    std::cout << "CRC OK" << std::endl;
                    ack_to_send ++;
                }

                rDataEncoded.clear();
                buffer.clear();
                state = 0;
            }
        }

    }


    ~Receiver() {


        std::cout << "Extracting ..." << std::endl;

        std::ofstream rDataFile {"rData.txt"};

        for (auto const &d : rDataEncoded)
            rDataFile << d << std::endl;

        // ----------------------------------------------------------------

        std::ofstream outputDataFile("OUTPUT.bin", std::ios::binary);
        char byteOut;
        for (auto const &byteOut : rData.as_span<char>())
            outputDataFile.write(&byteOut, sizeof(byteOut));

    }
};


int main() {

    auto receiver = std::make_shared<Receiver>();
    auto device = std::make_shared<Device>("sSignal.txt", 512);
    device->open();

    std::cout << std::endl;
    std::cout << "Starting device ..." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(1));
    device->start(receiver);
    std::this_thread::sleep_for(std::chrono::seconds(60));
    device->stop();


    return 0;
}

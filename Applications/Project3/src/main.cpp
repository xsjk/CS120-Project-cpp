#include <iostream>
#include <cstring>
#include <queue>
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
#include <boost/json.hpp>
#include <sstream>

#include "asyncio.hpp"
#include "signal.hpp"
#include "CRC.hpp"
#include "device.hpp"

using namespace utils;

namespace OSI {


    using ByteStreamBuf = boost::asio::streambuf;


    class AsyncPhysicalLayer : public IOHandler<float> {

        void outputCallback(DataView<float> &view) noexcept override {

            assert(view.getNumChannels() == 2);
            auto p = std::span(boost::asio::buffer_cast<const float *>(sSignalBuffer.data()), sSignalBuffer.size() / sizeof(float));
            for (auto i = 0; i < view.getNumSamples(); i++)
                view(0, i) = view(1, i) = i < p.size() ? p[i] : 0;
            sSignalBuffer.consume(view.getNumSamples() * sizeof(float));

        }

        void inputCallback(DataView<float> &&view) noexcept override {

            assert(view.getNumChannels() == 1);
            auto p = std::span(boost::asio::buffer_cast<float *>(rSignalBuffer.prepare(view.getNumSamples() * sizeof(float))), view.getNumSamples());
            for (auto i = 0; i < p.size(); i++)
                p[i] = view(0, i);
            rSignalBuffer.commit(view.getNumSamples() * sizeof(float));

            // process received signal in the receiver context (in another thread)
            // so that the inputCallback will not be blocked
            boost::asio::post(receiverContext, [&] {

                static int fromLastPreamble = 0;
                static std::deque<float> rSignalQueue;
                static std::ofstream rSignalFile { "rSignal.txt" };
                static BitsContainer rDataEncoded;
                static CRC8<0x7> CRCChecker;
                static enum class ReceiveState : char {
                    preambleDetection,
                    dataExtraction
                } receiveState = ReceiveState::preambleDetection;


                auto p = std::span(boost::asio::buffer_cast<const float *>(rSignalBuffer.data()), rSignalBuffer.size() / sizeof(float));

                float sum = 0;
                for (auto t = 0; t < p.size(); t++, fromLastPreamble++) {
                    rSignalFile << p[t] << '\n';
                    rSignalQueue.push_back(p[t]);
                    switch (receiveState) {
                        case ReceiveState::preambleDetection:
                            if (rSignalQueue.size() < preamble.size())
                                continue;
                            sum = 0;
                            for (auto tt = 0; tt < preamble.size(); tt++)
                                sum += rSignalQueue[tt] * preamble[tt];
                            rSignalQueue.pop_front();
                            if (sum > threshold && fromLastPreamble > preamble.size()) {
                                fromLastPreamble = 0;
                                rSignalQueue.clear();
                                receiveState = ReceiveState::dataExtraction;
                            }
                            break;
                        case ReceiveState::dataExtraction:
                            {
                                auto p = boost::asio::buffer_cast<uint8_t *>(rDataBuffer.prepare(packetBits / 8));
                                if (rSignalQueue.size() < packetBits * carrierSize)
                                    continue;
                                auto i = 0;
                                CRCChecker.reset();

                                enum class Receiving : char { len, data, crc } cur = Receiving::len;
                                payload_size_t real_payload = 0;
                                for (auto t = 0; t < std::min<int>(packetBits, (sizeof(payload_size_t) + real_payload + 1) * 10) * carrierSize ; ) {
                                    sum = 0;
                                    for (auto tt = 0; tt < carrierSize; t++, tt++)
                                        sum += rSignalQueue[t] * carrier[tt];
                                    rDataEncoded.push_back(sum < 0);
                                    if (rDataEncoded.size() % 10 == 0) {
                                        auto lastIndex = rDataEncoded.size() / 10 - 1;
                                        auto bitset = B8B10::decode(rDataEncoded.get<10>(lastIndex));
                                        auto byte = (uint8_t)bitset.to_ulong();
                                        switch (cur) {
                                            case Receiving::len:
                                                real_payload |= (payload_size_t)byte << 8 * lastIndex;
                                                if (rDataEncoded.size() / 10 == sizeof(payload_size_t)) {
                                                    rDataEncoded.clear();
                                                    cur = Receiving::data;
                                                }
                                                break;
                                            case Receiving::data:
                                                CRCChecker.update(p[i++] = byte);
                                                rDataEncoded.clear();
                                                break;
                                        }
                                    }
                                }
                                if(i != real_payload + 1) {
                                    std::cerr << "Payload size error: " << i << " " << real_payload << std::endl;
                                } else if (CRCChecker.q == 0) {
                                    rDataBuffer.commit(real_payload);
                                } else {
                                    std::cout << "CRC failed" << std::endl;
                                }
                                rSignalQueue.clear();
                                receiveState = ReceiveState::preambleDetection;
                            }
                            break;
                    }
                }

                rSignalBuffer.consume(p.size() * sizeof(float));

            });
        }


        const float amplitude;  // amplitude of the sending signal
        const float threshold;  // threshold for preamble detection
        const int payload;      // bytes per CRC check
        const int packetBits;   // bits per packet (calculated by payload)
        const int carrierSize;  // size of carrier
        const int interSize;    // size of interval between packets

        /*
            | preamble | length | data | crc |
        */
       using payload_size_t = uint16_t;
       static_assert(std::is_unsigned_v<payload_size_t>);

        std::vector<float> preamble, carrier;

        ByteStreamBuf sSignalBuffer, rSignalBuffer, sDataBuffer, rDataBuffer;
        Context senderContext, receiverContext;


        def send_raw(BitsContainer &&rawBits) {

            static std::ofstream sSignalFile { "sSignal.txt" };

            std::cout << "Generating sSignal ..." << std::endl;

            auto nBits = rawBits.size();
            auto nPacket = nBits / packetBits;
            auto tickPerPacket = preamble.size() + packetBits * carrierSize + interSize * 2;
            auto nticks = tickPerPacket * nPacket;

            auto p = boost::asio::buffer_cast<float *>(sSignalBuffer.prepare(tickPerPacket * (nPacket + 1) * sizeof(float)));
            auto t = 0;
            for (auto i = 0; i < nBits; i += packetBits) {
                for (auto j = 0; j < interSize; j++)
                    p[t++] = 0;
                for (auto j = 0; j < preamble.size(); j++)
                    p[t++] = preamble[j] * amplitude;
                for (auto j = 0; j < packetBits && i + j < nBits; j++)
                    for (auto k = 0; k < carrierSize; k++)
                        p[t++] = carrier[k] * (rawBits[i + j] == 0 ? amplitude : -amplitude);
                for (auto j = 0; j < interSize; j++)
                    p[t++] = 0;
            }
            for (auto i = 0; i < t; i++)
                sSignalFile << p[i] << '\n';
            sSignalBuffer.commit(t * sizeof(float));

        }

    public:

        struct Config {
            float amplitude;
            float threshold;
            int payload;
            int carrierSize;
            int interSize;
            std::string preambleFile;
        };

        AsyncPhysicalLayer(Config c)
          : amplitude(c.amplitude),
            threshold(c.threshold),
            payload(c.payload),
            packetBits((c.payload + 1 + sizeof(payload_size_t)) * 10), // +1 for length
            carrierSize(c.carrierSize),
            interSize(c.interSize),
            preamble(from_file<float>(c.preambleFile)),
            carrier(c.carrierSize, 1.f) {
                if (packetBits % 8 != 0) {
                    auto corrected_payload = packetBits / 40 * 4 - 1 - sizeof(payload_size_t);
                    throw std::runtime_error(std::format(
                        "Invalid argument \"payload\", the \"packetBits\" should be the multiple of 8, "
                        "got packetBits = {}. The most likely available \"payload\" are {} and {}.",
                        payload, corrected_payload, corrected_payload + 4
                    ));
                }
                if (payload >= std::numeric_limits<payload_size_t>::max()) {
                    throw std::runtime_error(std::format(
                        "Invalid argument \"payload\", \"payload\" should be smaller than {}, got payload = {}", 
                        std::numeric_limits<payload_size_t>::max(), payload
                    ));
                }
            }

        /**
         * @brief send bits in the BitContainer
         *
         * @param data
         */
        async def async_send(BitsContainer &&data) -> awaitable<void> {
            co_await boost::asio::co_spawn(senderContext, [&](BitsContainer &&data) -> awaitable<void> {

                // the result after adding CRC and applying 8B10B
                BitsContainer rawBits;
                auto real_payload = payload;
                for (auto i = 0; i < data.size() / 8; i++) {
                    // calculate real payload are the beginning of the package
                    if (i % payload == 0) {
                        real_payload = std::min<int>(data.size() / 8 - i, payload);
                        for (int j = 0; j < sizeof(payload_size_t); j++)
                            rawBits.push(B8B10::encode(std::bitset<8>(
                                real_payload >> 8 * j
                            )));
                    }
                        
                    // apply 8B10B to data bits
                    rawBits.push(B8B10::encode(data.get<8>(i)));
                    // add CRC at the end of the package
                    if ((i + 1 + payload - real_payload) % payload == 0)
                        rawBits.push(B8B10::encode(std::bitset<8>(CRC8<7>::get(data.as_span<uint8_t>().subspan(
                            (i / payload) * payload, real_payload
                        )))));
                }

                send_raw(std::move(rawBits));
                co_return;
            }(std::move(data)), boost::asio::use_awaitable);
            co_return;
        }


        /**
         * @brief send bits from the send buffer.
         *
         * @param sendbuf
         */
        async def async_send(ByteStreamBuf &sendbuf) -> awaitable<void> {
            co_await boost::asio::co_spawn(senderContext, [&](ByteStreamBuf &sendbuf) -> awaitable<void> {
                auto q = std::span(boost::asio::buffer_cast<const uint8_t *>(sendbuf.data()), sendbuf.size());
                auto real_payload = payload;
                BitsContainer rawBits;
                for (auto i = 0; i < q.size() / 8; i++) {
                    if (i % payload == 0)
                        rawBits.push(B8B10::encode(std::bitset<8>(
                            real_payload = std::min<int>(q.size() / 8 - i, payload)
                        )));
                    rawBits.push(B8B10::encode(std::bitset<8>(q[i])));
                    if ((i + 1 + payload - real_payload) % payload == 0)
                        rawBits.push(B8B10::encode(std::bitset<8>(CRC8<7>::get(
                            q.subspan((i / payload) * payload, payload)
                        ))));
                }
                sendbuf.consume(q.size());
                send_raw(std::move(rawBits));
                co_return;
            }(sendbuf), boost::asio::use_awaitable);
            co_return;
        }


        /**
         * @brief read bits, save to the read buffer.
         *
         * @param readbuf
         */
        async def async_read(ByteStreamBuf &readbuf) -> awaitable<void> {
            co_await boost::asio::co_spawn(receiverContext, [&](ByteStreamBuf &readbuf) -> awaitable<void> {
                auto p = std::span(boost::asio::buffer_cast<char *>(readbuf.prepare(rDataBuffer.size())), rDataBuffer.size());
                auto q = std::span(boost::asio::buffer_cast<const char *>(rDataBuffer.data()), rDataBuffer.size());
                for (auto i = 0; i < p.size(); i++)
                    p[i] = q[i];
                rDataBuffer.consume(p.size());
                readbuf.commit(p.size());
                co_return;
            }(readbuf), boost::asio::use_awaitable);
        }

        /**
         * @brief read bits, return a BitsContainer
         *
         * @return bitsContainer
         */
        async def async_read() -> awaitable<BitsContainer> {
            co_return co_await boost::asio::co_spawn(receiverContext, [&]() -> awaitable<BitsContainer> {
                BitsContainer rData;
                auto p = std::span(boost::asio::buffer_cast<const char *>(rDataBuffer.data()), rDataBuffer.size());
                for (auto i = 0; i < p.size(); i++)
                    rData.push(p[i]);
                rDataBuffer.consume(p.size());
                co_return rData;
            }(), boost::asio::use_awaitable);
        }


    };


}


#include "argparse/argparse.hpp"

int main(int argc, char **argv) {

    return asyncio.run([&]() -> awaitable<int> {

        argparse::ArgumentParser program("test");
        program.add_argument("-c", "--configPath").default_value("config.json");

        try {
            program.parse_args(argc, argv);
        }
        catch (const std::exception &err) {
            std::cerr << err.what() << std::endl;
            std::cerr << program;
        }

        auto configPath = program.get<std::string>("--configPath");
        std::ifstream jsonFile(configPath);
        std::stringstream jsonFileBuffer;
        jsonFileBuffer << jsonFile.rdbuf();
        jsonFile.close();

        auto parsed = boost::json::parse(jsonFileBuffer.str());
        auto& configObj = parsed.as_object();

        try {

            auto inputFile = std::string(configObj.at("inputFile").as_string());
            auto outputFile = std::string(configObj.at("outputFile").as_string());

            std::cout << "Input file: " << inputFile << std::endl;
            std::cout << "Output file: " << outputFile << std::endl;

            using namespace std::chrono_literals;

            auto physicalLayer = std::make_shared<OSI::AsyncPhysicalLayer>(
                OSI::AsyncPhysicalLayer::Config {
                    .amplitude = (float)configObj.at("amplitude").as_double(),
                    .threshold = (float)configObj.at("threshold").as_double(),
                    .payload = (int)configObj.at("payload").as_int64(),
                    .carrierSize = (int)configObj.at("carrierSize").as_int64(),
                    .interSize = (int)configObj.at("interSize").as_int64(),
                    .preambleFile = std::string(configObj.at("preambleFile").as_string())
                });

            // --------------------- send bits ---------------------
            std::cout << std::endl;
            co_await physicalLayer->async_send(BitsContainer::from_bin(inputFile));

            // ------------------ start the device and send ------------------
            auto device = std::make_shared<Device>();
            device->open(1, 2);


            std::cout << std::endl;
            std::cout << "Starting ..." << std::endl;
            co_await asyncio.sleep(1s);

            device->start(physicalLayer);

            std::cout << "Sending ..." << std::endl;
            co_await asyncio.sleep(std::chrono::milliseconds((int)(1000 * configObj.at("time").as_double())));

            device->stop();

            // --------------------- save data to file ---------------------

            std::cout << "Saving ..." << std::endl;
            auto rData = co_await physicalLayer->async_read();
            rData.to_bin(outputFile);
            rData.to_file("rData.txt");
            co_return 0;

        } catch (const std::exception &err) {
            std::cerr << err.what() << std::endl;
            co_return -1;
        }

    }());

}

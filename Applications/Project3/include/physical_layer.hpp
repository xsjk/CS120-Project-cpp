#pragma once

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
                static std::ofstream rSignalFile { "rSignal.txt" };
                static CRC8<0x7> CRCChecker;
                static enum class ReceiveState : char {
                    preambleDetection,
                    dataExtraction
                } receiveState = ReceiveState::preambleDetection;

                auto rSignal = std::span(boost::asio::buffer_cast<const float *>(rSignalBuffer.data()), rSignalBuffer.size() / sizeof(float));

                for (auto t = 0; t < rSignal.size(); t++, fromLastPreamble++) {
                    rSignalFile << rSignal[t] << '\n';
                    switch (receiveState) {
                        case ReceiveState::preambleDetection:
                            {
                                static std::deque<float> rSignalQueue;
                                rSignalQueue.push_back(rSignal[t]);
                                if (rSignalQueue.size() < preamble.size())
                                    continue;
                                float sum = 0;
                                for (auto tt = 0; tt < preamble.size(); tt++)
                                    sum += rSignalQueue[tt] * preamble[tt];
                                rSignalQueue.pop_front();
                                if (sum > threshold && fromLastPreamble > preamble.size()) {
                                    fromLastPreamble = 0;
                                    rSignalQueue.clear();
                                    receiveState = ReceiveState::dataExtraction;
                                }
                            }
                            break;
                        case ReceiveState::dataExtraction:
                            {
                                static enum class Receiving : char { len, data, crc } cur = Receiving::len;
                                static payload_size_t real_payload = 0;
                                static BitsContainer rDataEncoded; // bits encoded with 8B10B and container length, data and crc.
                                static ByteContainer rDataDecoded; // bytes only contains decoded data

                                // get rDataEncoded from rSignal                                
                                static auto dt = 0;
                                static float sum = 0;
                                sum += rSignal[t] * carrier[dt];
                                dt++;
                                if (dt % carrierSize == 0) {
                                    rDataEncoded.push_back(sum < 0);
                                    sum = 0;
                                    dt = 0;

                                    if (rDataEncoded.size() > 0 && rDataEncoded.size() % 10 == 0) {
                                        auto lastrDataIndex = rDataEncoded.size() / 10 - 1;
                                        uint8_t byte;
                                        try {
                                            byte = (uint8_t)B8B10::decode(rDataEncoded.get<10>(lastrDataIndex)).to_ulong();
                                        } catch (const std::exception& e) {
                                            std::cerr << "8B10B decode error" << std::endl;
                                            receiveState = ReceiveState::preambleDetection;
                                            continue;
                                        }
                                        switch (cur) {
                                            case Receiving::len:
                                                real_payload |= (payload_size_t)byte << 8 * lastrDataIndex;
                                                if (rDataEncoded.size() / 10 == sizeof(payload_size_t)) {
                                                    if (real_payload > 0) {
                                                        cur = Receiving::data;
                                                        rDataDecoded.clear();
                                                    } else {
                                                        std::cerr << "Payload Error: " << real_payload << std::endl;
                                                        receiveState = ReceiveState::preambleDetection;
                                                    }
                                                }
                                                break;
                                            case Receiving::data:
                                                rDataEncoded.clear();
                                                CRCChecker.update(byte);
                                                rDataDecoded.push_back(byte);
                                                if (rDataDecoded.size() == real_payload)
                                                    cur = Receiving::crc;
                                                break;
                                            case Receiving::crc:
                                                rDataEncoded.clear();
                                                CRCChecker.update(byte);
                                                if (CRCChecker.q == 0) {
                                                    // CRC OK    
                                                    auto q = boost::asio::buffer_cast<uint8_t *>(rDataBuffer.prepare(rDataDecoded.size()));
                                                    for (auto i = 0; i < rDataDecoded.size(); i++)
                                                        q[i] = rDataDecoded[i];
                                                    rDataBuffer.commit(rDataDecoded.size());
                                                } else {
                                                    // CRC FAILED
                                                    std::cerr << "CRC failed" << std::endl;
                                                }
                                                cur = Receiving::len;
                                                real_payload = 0;
                                                rDataDecoded.clear();
                                                receiveState = ReceiveState::preambleDetection;
                                                break;
                                        }
                                        
                                    }
                                }
                                

                            }
                            break;
                    }
                }

                rSignalBuffer.consume(rSignal.size() * sizeof(float));

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

        /**
         * @brief async wait for rData to arrive
         * 
         */
        async def wait_data() -> awaitable<std::span<const uint8_t>> {
            while (rDataBuffer.size() == 0)
                co_await asyncio.sleep(0s);     // yield the coroutine
            co_return std::span(boost::asio::buffer_cast<const uint8_t *>(rDataBuffer.data()), rDataBuffer.size());
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
                auto q = co_await wait_data();
                auto p = std::span(boost::asio::buffer_cast<uint8_t *>(readbuf.prepare(q.size())), q.size());
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
                auto p = co_await wait_data();
                BitsContainer rData;
                for (auto i = 0; i < p.size(); i++)
                    rData.push(p[i]);
                rDataBuffer.consume(p.size());
                co_return rData;
            }(), boost::asio::use_awaitable);
        }


    };


}

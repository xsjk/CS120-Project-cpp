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

#define DEBUG

namespace OSI {


    using ByteStreamBuffer = boost::asio::streambuf;


    class AsyncPhysicalLayer : public IOHandler<float> {

        bool busy = false; // whether the channel is busy
        bool sending = false;

        void outputCallback(DataView<float> &view) noexcept override {

            assert(view.getNumChannels() == 2);
            auto p = std::span(boost::asio::buffer_cast<const float *>(sSignalBuffer.data()), sSignalBuffer.size() / sizeof(float));
            auto front_packet_size = sSignalBuffer.front_packet_size() / sizeof(float);
            auto n_samples = view.getNumSamples();
            auto consume_size = 0;

            if (busy) {
                if (sending) {
                    if (front_packet_size < n_samples) {
                        consume_size = front_packet_size;
                        sending = false;
                    } else {
                        consume_size = n_samples;
                    }
                } else {
                    consume_size = 0;
                }
            } else {
                if (p.size() < n_samples) {
                    consume_size = p.size();
                    sending = false;
                } else {
                    consume_size = n_samples;
                    sending = true;
                }
            }

            for (auto i = 0; i < n_samples; i++)
                view(0, i) = view(1, i) = i < consume_size ? p[i] : 0;
            sSignalBuffer.consume(consume_size * sizeof(float));

        }

        void inputCallback(DataView<float> &&view) noexcept override {

            assert(view.getNumChannels() == 1);
            auto p = std::span(boost::asio::buffer_cast<float *>(rSignalBuffer.prepare(view.getNumSamples() * sizeof(float))), view.getNumSamples());
            float sum = 0;
            for (auto i = 0; i < p.size(); i++) {
                p[i] = view(0, i);
                sum += p[i] * p[i];
            }
            rSignalBuffer.commit(view.getNumSamples() * sizeof(float));

            busy = sum > busy_threshold;

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
                                static Header header;               // physical layer header
                                static BitsContainer rDataEncoded;  // bits encoded with 8B10B and container length, data and crc.
                                static ByteContainer rDataDecoded;  // bytes only contains decoded data

                                // get rDataEncoded from rSignal
                                static auto dt = 0;
                                static float sum = 0;
                                static bool is_last_packet = false;
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
                                            auto encoded = rDataEncoded.get<10>(lastrDataIndex);
                                            auto decoded = B8B10::decode(encoded);
                                            byte = (uint8_t)decoded.to_ulong();
                                        } catch (const std::exception& e) {
                                            // misdetection of preamble
                                            #ifdef DEBUG
                                                std::cerr << "8B10B decode failed" << std::endl;
                                            #endif
                                            cur = Receiving::len;
                                            rDataEncoded.clear();
                                            receiveState = ReceiveState::preambleDetection;
                                            continue;
                                        }
                                        switch (cur) {
                                            case Receiving::len:
                                                ((char*)&header)[lastrDataIndex] = byte;
                                                if (rDataEncoded.size() / 10 == sizeof(Header)) {
                                                    if (header.size > 0) {
                                                        is_last_packet = header.done;
                                                        cur = Receiving::data;
                                                        CRCChecker.reset();
                                                        rDataDecoded.clear();
                                                    } else {
                                                        std::cerr << "Payload Error: " << header.size << std::endl;
                                                        receiveState = ReceiveState::preambleDetection;
                                                    }
                                                }
                                                break;
                                            case Receiving::data:
                                                rDataEncoded.clear();
                                                CRCChecker.update(byte);
                                                rDataDecoded.push_back(byte);
                                                if (rDataDecoded.size() == header.size)
                                                    cur = Receiving::crc;
                                                break;
                                            case Receiving::crc:
                                                rDataEncoded.clear();
                                                CRCChecker.update(byte);
                                                if (CRCChecker.q == 0) {
                                                    // CRC OK
                                                    static ByteContainer rDataBuffer;
                                                    for (auto i = 0; i < rDataDecoded.size(); i++)
                                                        rDataBuffer.push(rDataDecoded[i]);
                                                    if (is_last_packet)
                                                        rPacketQueue.push(std::move(rDataBuffer));
                                                } else {
                                                    // CRC FAILED
                                                    #ifdef DEBUG
                                                        std::cerr << "CRC failed" << std::endl;
                                                        std::cout << rDataDecoded << std::endl;
                                                    #endif
                                                }
                                                cur = Receiving::len;
                                                header.size = 0;
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
        const float busy_threshold;  // threshold for busy detection
        const int payload;      // bytes per CRC check
        const int packetBits;   // bits per packet (calculated by payload)
        const int carrierSize;  // size of carrier
        const int interSize;    // size of interval between packets

        /*
            | preamble | size | done | data | crc |
        */
        struct Header {
            unsigned size: 31;
            unsigned done: 1;
        };
        static_assert(sizeof(Header) == 4);

        std::vector<float> preamble, carrier;

        PacketStreamBuffer sSignalBuffer;
        ByteStreamBuffer rSignalBuffer, sDataBuffer;
        ThreadSafeQueue<ByteContainer> rPacketQueue;

        Context senderContext, receiverContext;


        def send_raw(BitsContainer &&rawBits) {

            assert(rawBits.size() % 10 == 0);

            static std::ofstream sSignalFile { "sSignal.txt" };
            rawBits.to_file("sData.txt");

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
         * @note you should call this funciton in receiverContext
         *       to ensure thread safety
         */
        async def wait_data() -> awaitable<ByteContainer> {
            while (rPacketQueue.empty())
                co_await asyncio.sleep(0s);     // yield the receiverContext
            co_return rPacketQueue.pop();
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
            busy_threshold(20),
            payload(c.payload),
            packetBits((c.payload + 1 + sizeof(Header)) * 10), // +1 for length
            carrierSize(c.carrierSize),
            interSize(c.interSize),
            preamble(from_file<float>(c.preambleFile)),
            carrier(c.carrierSize, 1.f) {
                if (packetBits % 8 != 0) {
                    auto corrected_payload = packetBits / 40 * 4 - 1 - sizeof(Header);
                    throw std::runtime_error(std::format(
                        "Invalid argument \"payload\", the \"packetBits\" should be the multiple of 8, "
                        "got packetBits = {}. The most likely available \"payload\" are {} and {}.",
                        payload, corrected_payload, corrected_payload + 4
                    ));
                }
                constexpr auto maxpayload = 1ull << (8 * sizeof(Header) - 1);
                if (payload >= maxpayload) {
                    throw std::runtime_error(std::format(
                        "Invalid argument \"payload\", \"payload\" should be smaller than {}, got payload = {}",
                        maxpayload, payload
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
                Header header;
                CRC8<7> CRCChecker;
                CRCChecker.reset();

                for (auto i = 0; i < data.size() / 8; i++) {
                    // calculate real payload at the beginning of the package
                    if (i % payload == 0) {
                        header.size = std::min<int>(data.size() / 8 - i, payload);
                        header.done = (data.size() / 8 - 1) / payload == i / payload;
                        for (int j = 0; j < sizeof(Header); j++) {
                            auto byte = ((char*)&header)[j];
                            auto encoded = B8B10::encode(byte);
                            rawBits.push(encoded);
                        }
                    }

                    // apply 8B10B to data bits
                    auto byte = data.get<8>(i);
                    CRCChecker.update(byte.to_ulong());
                    rawBits.push(B8B10::encode(byte));

                    // add CRC at the end of the package
                    if ((i + 1 + payload - header.size) % payload == 0) {
                        rawBits.push(B8B10::encode(std::bitset<8>(CRCChecker.get())));
                        CRCChecker.reset();
                    }
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
        async def async_send(ByteStreamBuffer &sendbuf) -> awaitable<void> {
            co_await boost::asio::co_spawn(senderContext, [&](ByteStreamBuffer &sendbuf) -> awaitable<void> {
                auto q = std::span(boost::asio::buffer_cast<const uint8_t *>(sendbuf.data()), sendbuf.size());
                BitsContainer rawBits;
                Header header;
                CRC8<7> CRCChecker;
                CRCChecker.reset();

                for (auto i = 0; i < q.size(); i++) {
                    if (i % payload == 0) {
                        header.size = std::min<int>(q.size() - i, payload);
                        header.done = (q.size() - 1) / payload == i / payload;
                        for (int j = 0; j < sizeof(Header); j++) {
                            auto byte = ((char*)&header)[j];
                            auto encoded = B8B10::encode(byte);
                            rawBits.push(encoded);
                        }
                    }

                    CRCChecker.update(q[i]);
                    rawBits.push(B8B10::encode(std::bitset<8>(q[i])));

                    if ((i + 1 + payload - header.size) % payload == 0) {
                        rawBits.push(B8B10::encode(std::bitset<8>(CRCChecker.get())));
                        CRCChecker.reset();
                    }
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
        async def async_read(ByteStreamBuffer &readbuf) -> awaitable<int> {
            co_await boost::asio::co_spawn(receiverContext, [&]() -> awaitable<int> {
                auto q = co_await wait_data();
                auto p = std::span(boost::asio::buffer_cast<uint8_t *>(readbuf.prepare(q.size())), q.size());
                for (auto i = 0; i < p.size(); i++)
                    p[i] = q[i];
                readbuf.commit(p.size());
                co_return p.size();
            }(), boost::asio::use_awaitable);
        }

        /**
         * @brief read bits, return a BitsContainer
         *
         * @return bitsContainer
         */
        async def async_read() {
            return boost::asio::co_spawn(receiverContext, wait_data(), boost::asio::use_awaitable);
        }


    };


}

#pragma once

#include "utils.hpp"
#include "asyncio.hpp"
#include "asiodevice.h"

using namespace ASIO;
using namespace utils;

#ifdef AETHERNET_EXPORTS
#   define AETHERNET_API __declspec(dllexport)
#else
#   define AETHERNET_API __declspec(dllimport)
#endif

#define DEBUG
namespace OSI {


    using ByteStreamBuffer = boost::asio::streambuf;


    class AETHERNET_API AsyncPhysicalLayer : public IOHandler<float> {

        bool busy = false; // whether the channel is busy
        bool sending = false;

        void outputCallback(DataView<float> &view) noexcept override;
        void inputCallback(DataView<float> &&view) noexcept override;

        const float amplitude;  // amplitude of the sending signal
        const float threshold;  // threshold for preamble detection
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


        void send_raw(BitsContainer &&rawBits);

        /**
         * @brief async wait for rData to arrive
         *
         * @note you should call this funciton in receiverContext
         *       to ensure thread safety
         */
        awaitable<ByteContainer> wait_data();
        
    public:

        struct Config {
            float amplitude;
            float threshold;
            int payload;
            int carrierSize;
            int interSize;
            std::string preambleFile;
        };

        AsyncPhysicalLayer(Config c);

        /**
         * @brief send bits in the BitContainer
         *
         * @param data
         */
        awaitable<void> async_send(BitsContainer &&data);


        /**
         * @brief send bits from the send buffer.
         *
         * @param sendbuf
         */
        awaitable<void> async_send(ByteStreamBuffer &sendbuf);


        /**
         * @brief read bits, save to the read buffer.
         *
         * @param readbuf
         */
        awaitable<int> async_read(ByteStreamBuffer &readbuf);

        /**
         * @brief read bits, return a BitsContainer
         *
         * @return bitsContainer
         */
        awaitable<ByteContainer> async_read();


    };


}

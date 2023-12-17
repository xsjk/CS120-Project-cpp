#include "wintundevice.hpp"
#include "argparse/argparse.hpp"

int main(int argc, char **argv) {

    auto node1 = std::make_shared<WinTUN::Device>(
        "node1", GUID { 0xdeadbabe, 0xcafe, 0xbeef, { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef } }
    )->open("172.18.1.3");

    auto node2 = std::make_shared<WinTUN::Device>(
        "node2", GUID { 0xdeadbabe, 0xcafe, 0xbeef, { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xef, 0xcd } }
    )->open("172.18.1.1");

    asyncio.run(asyncio.gather([&]() -> awaitable<void> {
        boost::asio::streambuf buf;
        while (true) {
            try {
                auto n = co_await node1->async_read(buf);
                auto p = buf.data();
                WinTUN::PrintPacket((const uint8_t *)(p.data()), p.size());
                co_await node2->async_send(buf);
            } 
            catch (const CancelledError &e) {
                break;
            }
        }
    }(), [&]() -> awaitable<void> {
        boost::asio::streambuf buf;
        while (true) {
            try {
                auto n = co_await node2->async_read(buf);
                auto p = buf.data();
                WinTUN::PrintPacket((const uint8_t *)(p.data()), p.size());
                co_await node1->async_send(buf);
            } 
            catch (const CancelledError &e) {
                break;
            }
        }
    }()));

    return 0;

}

#include "wintundevice.hpp"
#include "argparse/argparse.hpp"

int main(int argc, char **argv) {

    auto session = std::make_shared<WinTUN::Device>(
        "node1", GUID { 0xdeadbabe, 0xcafe, 0xbeef, { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef } }
    )->open("10.6.7.7");

    asyncio.run([&]() -> awaitable<void> {
        while (true) {
            try {
                boost::asio::streambuf buf;
                auto n = co_await session->async_read(buf);
                auto p = buf.data();
                WinTUN::PrintPacket((const uint8_t *)(p.data()), p.size());
            } 
            catch (const CancelledError &e) {
                break;
            }
        }
    }());

    return 0;

}

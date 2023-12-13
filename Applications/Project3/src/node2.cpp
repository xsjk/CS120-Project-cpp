#include "wintundevice.hpp"
#include "argparse/argparse.hpp"

int main(int argc, char **argv) {

    auto session = std::make_shared<WinTUN::Device>(
        "node2", GUID { 0xdeadbabe, 0xcafe, 0xbeef, { 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe } }
    )->open("10.6.7.8");

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

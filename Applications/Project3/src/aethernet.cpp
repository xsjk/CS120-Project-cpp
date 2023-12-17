#include "argparse/argparse.hpp"
#include "physical_layer.hpp"
#include "wintundevice.hpp"
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
        if (!jsonFile.is_open()) {
            std::cerr << "Cannot open config file: " << configPath << std::endl;
            co_return -1;
        }
        std::stringstream jsonFileBuffer;
        jsonFileBuffer << jsonFile.rdbuf();
        jsonFile.close();

        auto parsed = boost::json::parse(jsonFileBuffer.str());
        auto& configObj = parsed.as_object();

        try {

            auto ip = std::string(configObj.at("ip").as_string());
            auto name = std::string(configObj.at("name").as_string());

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

            auto session = std::make_shared<WinTUN::Device>(
                name, GUID { 0xdeadbabe, 0xcafe, 0xbeef, { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef } }
            )->open(ip);

            // ------------------ start the device and send ------------------
            auto device = std::make_shared<Device>();
            device->open(1, 2);

            std::cout << std::endl;
            std::cout << "Starting ..." << std::endl;
            co_await asyncio.sleep(1s);

            device->start(physicalLayer);
            
            co_await asyncio.gather(
                [&] () -> awaitable<void> {
                    OSI::ByteStreamBuffer buf;

                    while (true) {
                        try {
                            auto n = co_await session->async_read(buf);
                            auto p = buf.data();
                            // WinTUN::PrintPacket((const uint8_t *)(p.data()), p.size());
                            co_await physicalLayer->async_send(buf);
                        } 
                        catch (const CancelledError &e) {
                            break;
                        }
                    }
                    co_return;
                    
                }(), [&] () -> awaitable<void> {
                    OSI::ByteStreamBuffer buf;

                    while (true) {
                        try {
                            auto n = co_await physicalLayer->async_read(buf);
                            auto p = buf.data();
                            WinTUN::PrintPacket((const uint8_t *)(p.data()), p.size());
                            co_await session->async_send(buf);
                        } 
                        catch (const CancelledError &e) {
                            break;
                        }
                    }
                    co_return;
                    
                }()
            );

            device->stop();

            co_return 0;

        } catch (const std::exception &err) {
            std::cerr << err.what() << std::endl;
            co_return -1;
        }

    }());

}

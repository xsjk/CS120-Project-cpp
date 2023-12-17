#include "physical_layer.hpp"
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
            OSI::ByteStreamBuffer buf;
            auto bits = BitsContainer::from_bin(inputFile);
            for (auto i = 0; i < bits.size() / 8; i++)
                buf.sputc(bits.get<8>(i).to_ulong());
            co_await physicalLayer->async_send(buf);

            // ------------------ start the device and send ------------------
            auto device = std::make_shared<Device>();
            device->open(1, 2);


            std::cout << std::endl;
            std::cout << "Starting ..." << std::endl;
            co_await asyncio.sleep(1s);

            device->start(physicalLayer);

            std::cout << "Sending ..." << std::endl;

            auto rData = co_await physicalLayer->async_read();
            device->stop();

            // --------------------- save data to file ---------------------
            std::cout << "Saving ..." << std::endl;
            rData.to_bin(outputFile);
            rData.to_file("rData.txt");

            co_return 0;

        } catch (const std::exception &err) {
            std::cerr << err.what() << std::endl;
            co_return -1;
        }

    }());

}

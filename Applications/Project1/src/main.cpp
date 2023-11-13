#include "physical.hpp"

namespace Physical {

    auto config = BitStreamDeviceConfig{
        .package_size = 8,
        // .preamble = SinePreamble { {
        //     .omega = 8000. / 48000.,
        //     .duration = 1000,
        //     .butter = {
        //         {0.00016822,  0.        , -0.00033645,  0.        ,  0.00016822},
        //         {1.        , -1.98165982,  2.94480658, -1.94530969,  0.96365298}
        //     }
        // } },
        .preamble = StaticPreamble("preamble.txt", 150),
        .modem = QAMModem { {
            .omega = 2400. / 48000.,
            .duration = 600,
            .butter = {
                { 0.00016822370859146914, 0.0, -0.0003364474171829383, 0.0, 0.00016822370859146914},
                { 1.0, -3.76934096654668, 5.515261708905102, -3.7001989105751507, 0.9636529842237052}
            },
            .order = 1,
        } }
    };
    
    struct BitStreamDevice {

        using IOHandler = BitStreamDeviceIOHandler<decltype(config.preamble), decltype(config.modem)>;

        std::shared_ptr<IOHandler> io = std::make_shared<IOHandler>(std::move(config));
        std::shared_ptr<Device> device;

        BitStreamDevice() {
            device = std::make_shared<Device>();
            device->open();
            device->start(io);
        }

        ~BitStreamDevice() {
            device->stop();
            device->close();
        }

        void send(const auto& data) { io->send(data); }
        bool available() { return io->available(); }
        auto read() { return io->read(); }
        
    } bitStreamDevice;

    struct SoundInStream {
        auto& operator>>(std::vector<bool> &data) {
            data = bitStreamDevice.read();
            return *this;
        }
    } sin;

    struct SoundOutBitStream {
        auto& operator<<(const std::vector<bool> &data) {
            bitStreamDevice.send(data);
            return *this;
        }
    } sout;
}


int main() {

    // std::ios::sync_with_stdio(false);
    // std::cin.tie(nullptr);
    // std::cout.tie(nullptr);

    std::this_thread::sleep_for(std::chrono::seconds(5));

    //3       6      3      4
    Physical::sout << std::vector<bool> {
        1, 0, 1, 1, 0, 0, 1, 1, 
        1, 1, 1, 0, 1, 1, 0, 0, 
        1, 1, 0, 1, 0, 0, 1, 1, 
        1, 0, 0, 1, 0, 1, 0, 1, 
        1, 0, 0, 0, 0, 1, 1, 1
        // 0,1, 0,0,
        // 1,1, 0,0, 0,0, 0,1,
        // 0,0, 1,1
    };
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::vector<bool> output;
    Physical::sin >> output;
    std::cout << "Received: " << output.size() << '\n';
    if (output.size() == 0) {
        std::cout << "No data received\n";
        return 0;
    }
    for (int i = 0; i < 40; i += 8) {
        for (int j = i; j < i+8; j++)
            std::cout << output[j] << ' ';
        std::cout << '\n';
    }
    
    // std::vector<bool> input, output;
    // std::ifstream ifile { "INPUT.txt" };
    // std::ofstream ofile { "OUTPUT.txt" };

    // bool bit;
    // for (int i = 0; i < 10000; i++) {
    //     ifile >> bit;
    //     input.push_back(bit);
    // }

    // Physical::sout << input;
    
    // std::this_thread::sleep_for(std::chrono::seconds(10));

    // Physical::sin >> output;
    // for (auto bit : output)
    //     ofile << bit << '\n';


    return 0;

}

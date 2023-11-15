#include "physical.hpp"
#include "callbacklayer.hpp"
#include "CRC.hpp"


struct PhysicalLayerHandler {
    using LowerData = DataView<float>;
    using UpperData = std::vector<bool>;
    
    void passUpper(const LowerData& input, UpperData& output) {
        // calculate the upper_input;
        // input -> upper_input
    }

    void passLower(const UpperData& input, LowerData& output) {
        // calculate the output of this layer based on the ouput of upper layer
        // upper_output -> ouput
    }
};

struct DataLinkLayerHandler {
    using LowerData = std::vector<bool>;
    using UpperData = std::vector<bool>;

    void passUpper(const LowerData& input, UpperData& output) {
        // calculate the upper_input;
        // input -> upper_input
    }

    void passLower(const UpperData& input, LowerData& output) {
        // calculate the output of this layer based on the ouput of upper layer
        // upper_output -> ouput
    }
};

struct DisplayLayerHandler {
    using LowerData = std::vector<bool>;
    using UpperData = void;

    void inputCallback(const LowerData& input) {
        for (auto bit : input)
            std::cout << bit;
    }
    void outputCallback(LowerData& output) {

    }
};


int main() {

    auto device = std::make_shared<Device>();
    auto io = std::make_shared<OSI::MultiLayerIOHandler<
        PhysicalLayerHandler, 
        DataLinkLayerHandler, 
        DisplayLayerHandler
    >>();
    device->open();
    device->start(io);

    std::this_thread::sleep_for(std::chrono::seconds(10));
    return 0;

}

#pragma once


#include <iostream>
#include <cstring>
#include <chrono>
#include <cmath>
#include <memory>
#include <utility>
#include <fstream>
#include <vector>
#include <algorithm>
#include <queue>
#include <limits>
#include <valarray>
#include <thread>
#include <mutex>
#include <optional>
#include <functional>
#include <iterator>
#include <filesystem>
#include <future>

#include "signal.hpp"

#include "device.hpp"
#include "preamble.hpp"

#include "sender.hpp"
#include "receiver.hpp"
#include "modem.hpp"

namespace Physical {
    
    template<typename PreambleType, typename ModemType>
    struct BitStreamDeviceConfig {
        int package_size;
        PreambleType preamble;
        ModemType modem;
    };

    template<typename PreambleType, typename ModemType>
    class BitStreamDeviceIOHandler : public std::enable_shared_from_this<BitStreamDeviceIOHandler<PreambleType, ModemType>>, public IOHandler<float> {

        BitStreamDeviceConfig<PreambleType, ModemType> config;
        Sender sender { config.modem, config.preamble, config.package_size};
        Receiver receiver { config.modem, config.preamble, config.package_size };

        bool running = false;

    public:
        constexpr BitStreamDeviceIOHandler(BitStreamDeviceConfig<PreambleType, ModemType>&& c) : config(std::move(c)) {}
        void inputCallback(const DataView<float> &p) noexcept override { receiver.handleCallback(p); }
        void outputCallback(DataView<float> &p) noexcept override { sender.handleCallback(p); }
        void send(const auto& data) { sender.send(data); }
        bool available() { return receiver.available(); }
        auto read() { return receiver.read(); }

    };


}

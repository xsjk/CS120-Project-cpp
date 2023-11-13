#pragma once

#include <iostream>

    
#include "preamble.hpp"
#include "modem.hpp"
#include "generator.hpp"

namespace Physical {

class Sender {

    GeneratorQueue<float> outputGenerator;
    std::mutex generator_mutex;
    Modem& modem;
    Preamble& preamble;
    #ifdef LOG
        std::ofstream ofile { "sender.log" };
    #endif
    int package_size;

public:

    Sender(Modem &modem, Preamble& preamble, int package_size)
            : modem(modem), preamble(preamble), package_size(package_size) {}

    void send(const std::vector<bool> &data) {
        std::lock_guard<std::mutex> lock(generator_mutex);
        
        int cur_package_index = 0;
        for (Modem::Symbol symbol: modem.bits_to_symbols(data)) {
            if (cur_package_index == package_size) {
                outputGenerator.push(modem.create_data_end());
                cur_package_index = 0;
            }
            if (cur_package_index == 0) {
                outputGenerator.push(preamble.create());
                outputGenerator.push(modem.create_calibrate());
            }
            outputGenerator.push(modem.create(symbol));
            cur_package_index++;
        }

        std::cout << "(Sender) Data Ready " << data.size() << " bits, " << outputGenerator.size() << " samples\n";

    }

    void handleCallback(DataView &p) noexcept {
        
        std::lock_guard<std::mutex> lock(generator_mutex);
        for (auto i = 0; i < p.getNumSamples(); i++) {
            float y = outputGenerator();
            p(0, i) = y;
            p(1, i) = y;
            #ifdef LOG
                ofile << y << std::endl;
            #endif
        }
    }
};

}
#pragma once

#include <queue>
#include <vector>
#include <fstream>
#include <mutex>

#include "modem.hpp"
#include "preamble.hpp"

namespace Physical {

class Receiver {
    enum class State {
        Calibrating,
        Idle,
        ModemCalibrating,
        FetchingData,
        Stop,

    } state = State::Calibrating;
    
    std::vector<bool> inputBits;

    std::vector<float> inputBuffer; // the buffer to store signal for symbol decoding

    Modem &modem;
    Preamble &preamble;

    #ifdef LOG
        std::ofstream ofile { "receiver.log" };
    #endif
    std::mutex mutex;
    int package_size;

public:

    Receiver(Modem &modem, Preamble &preamble, int package_size) : modem(modem), preamble(preamble), package_size(package_size) {}

    bool available() {
        return !inputBits.empty();
    }

    // clear inputBits and return the data
    std::vector<bool> read() {
        return std::move(inputBits);
    }

    int cur_package_index = 0;
    void handleCallback(const DataView<float> &p) noexcept {

        std::lock_guard<std::mutex> lock(mutex);

        #ifdef LOG
            for (int i = 0; i < p.getNumSamples(); i++)
                ofile << p(0, i) << std::endl;
        #endif

        
        switch (state) {
            case State::Calibrating:
                if (preamble.calibrate(p)) {
                    state = State::Idle;
                    std::cout << "(Receiver) Calibration finished" << '\n';
                }
                break;

            case State::Idle:
                modem.reset();
                if (auto start = preamble.wait(p)) {
                    // std::cout << "(Receiver) Preamble end at " << *start << '\n';
                    state = State::FetchingData;
                    inputBuffer.clear();
                    for (int i = *start; i < std::min(
                        modem.symbol_duration - 
                        int(inputBuffer.size()),
                        int(p.getNumSamples())
                    ); i++) {
                        inputBuffer.push_back(p(0, i));
                    }
                }
                break;
                
            case State::FetchingData:
                {
                    int i_start = 0;

                    while (true) {

                        int i_max = std::min(
                            modem.symbol_duration - inputBuffer.size() + i_start,
                            p.getNumSamples()
                        );
                        
                        for (int i = i_start; i < i_max; i++)
                            inputBuffer.push_back(p(0, i));

                        if (inputBuffer.size() > modem.symbol_duration) {
                            // std::cerr << "(Receiver) Error: inputBuffer overflow" << '\n';
                            exit(-1);
                        } else if (inputBuffer.size() == modem.symbol_duration) {
                            Modem::Symbol symbol = modem.decode(inputBuffer);
                            // handle symbol 
                            switch (modem.symbol_type(symbol)) {
                                case Modem::SymbolType::Data:
                                    // std::cout << "(Receiver) Data symbol: " << (int)symbol << '\n';

                                    // transform symbol to bits and extend inputBits
                                    modem.symbol_to_bits(symbol, inputBits);

                                    if (++cur_package_index == package_size) {
                                        // std::cout << "(Receiver) Package finished" << '\n';
                                        cur_package_index = 0;
                                        state = State::Idle;
                                    }
                                    break;
                                case Modem::SymbolType::Stop:
                                    // std::cout << "(Receiver) Stop symbol " << (int)symbol << '\n';
                                    state = State::Idle;
                                    break;
                                case Modem::SymbolType::Error:
                                    // std::cerr << "(Receiver) Error symbol: " << (int)symbol << '\n';
                                    break;
                                case Modem::SymbolType::Pass:
                                    // std::cout << "(Receiver) Pass symbol" << '\n';
                                    break;
                            }

                            inputBuffer.clear();
                            i_start = i_max;

                            if (state != State::FetchingData)
                                break;
                            
                        } else {
                            break;
                            
                        }
                        
                    }
                }
                break;
            case State::Stop:
                break;
        }
    
    }

};


}
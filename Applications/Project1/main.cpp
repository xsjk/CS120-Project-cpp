#include <iostream>
#include <cstring>
#include <chrono>
#include <cmath>
#include <memory>
#include <utility>
#include <fstream>
#include <vector>
#include <mutex>
#include <algorithm>
#include <queue>
#include <limits>
#include <valarray>
#include <thread>
#include <mutex>
#include <optional>
#include <functional>
#include <iterator>

#include "signal.hpp"
// #include "wasapidevice.hpp"
// using namespace WASAPI;
#include "asiodevice.h"
using namespace ASIO;


float mean(auto v) {
    float sum = 0;
    for (auto x : v) sum += x;
    return sum / v.size();
}

float mean(auto v, auto f) {
    float sum = 0;
    for (auto x : v) sum += f(x);
    return sum / v.size();
}

float var(auto v) {
    float m = mean(v);
    float sum = 0;
    for (auto x : v) sum += (x - m) * (x - m);
    return sum / v.size();
}


namespace Project1 {

    constexpr int count_bits(int n) {
        int count = 0;
        while (n) {
            count++;
            n >>= 1;
        }
        return count;
    }

    template<typename T>
    class Generator {
        std::function<T(int)> func;
        int ticks;
        int current_tick = 0;
    public:
        std::string name;
        Generator(std::function<T(int)> func, int ticks, std::string name = "")
            : func(std::move(func)), ticks(ticks), name(std::move(name)) { }

        const Generator& operator=(const Generator&) = delete;

        T next() { 
            // if (current_tick == 0) 
            //     std::cout << "(Sender) " << name << " started " << std::endl;
            return func(current_tick++); 
        }
        bool empty() { return current_tick >= ticks; }
        int size() { return ticks - current_tick; }
        T operator()() { return next(); }
        operator bool() { return !empty(); }

    };

    template<typename T>
    class GeneratorQueue {
        std::queue<Generator<T>> queue;
        std::mutex mutex;
        int ticks = 0;
    public:

        void push(Generator<T> gen) {
            std::lock_guard<std::mutex> lock(mutex);
            queue.push(std::move(gen));
            ticks += queue.front().size();
        }

        Generator<T> pop() {
            std::lock_guard<std::mutex> lock(mutex);
            auto gen = queue.front();
            queue.pop();
            ticks -= gen.size();
            return gen;
        }

        bool empty() {
            return queue.empty();
        }

        int size() {
            return ticks;
        }

        T next() {
            if (queue.empty())
                return 0;
            auto& gen = queue.front();
            if (gen.empty()) {
                pop();
                return next();
            }
            ticks -= 1;
            return gen.next();
        }

        T operator()() { return next(); }
    };


    class BitStreamDeviceIOHandler : public std::enable_shared_from_this<BitStreamDeviceIOHandler>, public IOHandler {

        struct BitStreamDeviceConfig {
        } config;


        struct Preamble {
            virtual Generator<float> create() noexcept = 0;
            virtual bool calibrate(const DataView& p) noexcept = 0;
            virtual std::optional<int> search_preamble(const DataView& p) noexcept = 0;
            virtual std::optional<int> preamble_stop(const DataView& p) noexcept = 0;
        };

        struct SinePreamble : Preamble {
            float omega;
            int duration;
            Signals::Butter<float> butter;
            std::vector<float> buffer;

            std::ofstream ofile { "out.txt" };


            struct Config {
                float omega;
                int duration;
                Signals::Butter<float> butter;
            };

            SinePreamble(Config config) : omega(config.omega), duration(config.duration), butter(config.butter) { }

            Generator<float> create() noexcept override {
                return Generator<float>(
                    [omega=this->omega](int i) { return std::sin(2 * std::numbers::pi * omega * i); },
                    duration,
                    "Preamble"
                );
            }


            // continuous 5 chunks of data with same amplitude
            float amplitude_threshold = 0;
            float calibrate_counter = 0;
            bool calibrate(const DataView& p) noexcept override {
                if (calibrate_counter++ < 100) {
                    auto filtered = butter.filter(p[0]);
                    float max_amplitude = 0;
                    for (int i = 0; i < filtered.size(); i++) {
                        max_amplitude = std::max(max_amplitude, std::abs(filtered[i])); 
                        ofile << filtered[i] << std::endl;
                    }
                    amplitude_threshold = std::max(amplitude_threshold, max_amplitude);
                    return false;
                } else {
                    amplitude_threshold *= 30;
                    std::cout << "(Receiver) Preamble Amplitude threshold: " << amplitude_threshold << std::endl;
                    return true;
                }
            }


            int preamble_start_frame = 0;
            int frame_from_preamble = 0;
            std::optional<int> search_preamble(const DataView& p) noexcept override {
                auto filtered = butter.filter(p[0]);
                std::optional<int> preamble_start_frame;
                for (auto i = 0; i < filtered.size(); i++) {
                    ofile << filtered[i] << std::endl;
                    if (!preamble_start_frame && std::abs(filtered[i]) > amplitude_threshold)
                        preamble_start_frame = i;
                        frame_from_preamble = p.getNumSamples() - i;
                }
                if (preamble_start_frame)
                    butter.clean();
                return preamble_start_frame;
            }

            std::optional<int> preamble_stop(const DataView& p) noexcept override {
                if (frame_from_preamble + p.getNumSamples() < duration) {
                    frame_from_preamble += p.getNumSamples();
                    return {};
                }
                else 
                    return duration - frame_from_preamble;

                auto filtered = butter.filter(p[0]);
                std::optional<int> preamble_end_frame;
                
                for (auto i = 0; i < filtered.size(); i+=8) {
                    float max_amplitude = 0;
                    for (auto j = i; j < i + 8; j++) {
                        ofile << filtered[j] << std::endl;
                        max_amplitude = std::max(max_amplitude, std::abs(filtered[j]));
                    }
                    if (!preamble_end_frame && max_amplitude < amplitude_threshold)
                        preamble_end_frame = i;
                    // if (max_amplitude > amplitude_threshold)
                    //     std::cout << "max_amplitude " << max_amplitude << " > " << amplitude_threshold << std::endl;
                }
                if (preamble_end_frame)
                    return preamble_end_frame;
                else
                    return -1;
            } 

        } preamble;

        struct ChirpPreamble : Preamble {
            /// NOT IMPLEMENTED
        };


        struct Modem {

            using Symbol = uint8_t;

            const int symbol_duration;
            const int symbol_count;

            Modem(int symbol_duration, int symbol_count)
                : symbol_duration(symbol_duration), symbol_count(symbol_count) { }

            virtual std::function<float(int)> encode(Symbol symbol) = 0;
            virtual Symbol decode(std::span<float>) = 0;

            Generator<float> create(Symbol symbol) {
                return {encode(symbol), symbol_duration, "Symbol " + std::to_string(symbol)};
            }

            virtual std::vector<bool> symbol_to_bits(Symbol symbol) = 0;
            virtual std::vector<Symbol> bits_to_symbols(const std::vector<bool>& bits) = 0;

            virtual Generator<float> create_calibrate() = 0;

            Symbol data_end_symbol() {
                return symbol_count - 1;
            }
            auto create_data_end() {
                return create(data_end_symbol());
            }

            enum class SymbolType {
                Data,
                Stop,
                Error,
                Pass,
            };
            
            virtual SymbolType symbol_type(Symbol symbol) {
                return SymbolType::Data;
            }

        };

        class FreqModem : public Modem {

            std::vector<float> omegas;
        
        public:
            
            FreqModem(std::vector<float> omegas, int symbol_duration)
                : Modem(symbol_duration, 1 << omegas.size()), omegas(omegas)
            {
                std::cout << "(Receiver) Set Symbol Frequncies: [";
                for (auto f : omegas)
                    std::cout << f << ", ";
                std::cout << "\b\b]\n";

                ofile.open("fft.log");
            }

            struct Config {
                float omega_min;
                float omega_max;
                int omega_count;
                int symbol_duration;
            };

            std::ofstream ofile;

            FreqModem(Config config)
                : FreqModem {
                [](auto omega_min, auto omega_max, auto omega_count) {
                    std::vector<float> omegas(omega_count);
                    for (int i = 0; i < omega_count; i++)
                    omegas[i] = omega_min + (omega_max - omega_min) * i / (omega_count - 1);
                    return omegas;
                } (config.omega_min, config.omega_max, config.omega_count),
                config.symbol_duration,
                } { }
            
            // std::vector<Generator<float>> create(const std::vector<bool>& data) override {
            //     std::vector<Generator<float>> funcs;
            //     int bit_per_symbol = std::log2(symbol_count - 1);
            //     for (int i = 0; i < data.size(); i += bit_per_symbol) {
            //         Modem::Symbol symbol = 0;
            //         for (int j = 0; j < bit_per_symbol; j++)
            //             symbol |= data[i + j] << j;
            //         funcs.emplace_back(encode(symbol), symbol_duration, "Symbol " + std::to_string(symbol));
            //     }
            //     return funcs;
            // }

            std::vector<bool> symbol_to_bits(Symbol symbol) {
                std::vector<bool> bits;
                int bit_per_symbol = std::log2(symbol_count - 1);
                for (int i = 0; i < bit_per_symbol; i++)
                    bits.push_back((symbol >> i) & 1);
                return bits;
            }

            std::vector<Symbol> bits_to_symbols(const std::vector<bool>& bits) {
                std::vector<Symbol> symbols;
                int bit_per_symbol = std::log2(symbol_count - 1);
                for (int i = 0; i < bits.size(); i += bit_per_symbol) {
                    Modem::Symbol symbol = 0;
                    for (int j = 0; j < bit_per_symbol && i + j < bits.size(); j++)
                        symbol |= bits[i + j] << j;
                    symbols.push_back(symbol);
                }
                return symbols;
            }

            std::function<float(int)> encode(Symbol symbol) {
                if (symbol > symbol_count)
                    throw std::runtime_error("Error: symbol out of range");
                
                return [symbol, omegas=this->omegas](int i) {
                    float v = 0;
                    for (int j = 0; j < omegas.size(); j++)
                        if (symbol & (1 << j))
                            v += std::sin(2 * std::numbers::pi * omegas[j] * i);
                    // v += std::sin(2 * std::numbers::pi * omegas[omegas.size()-1] * i);
                    return v / omegas.size();
                };
            }


            Symbol decode(std::span<float> y) {
                auto n = y.size();
                auto y_ = std::vector<float>(y.begin(), y.end());
                Signals::fft<float>(y_);

                for (auto i : y_)
                    ofile << i << ' ';
            
                ofile << '\n';
                std::vector<float> amp(omegas.size());
                /// iterate over freqs and get amplitude
                Symbol s = 0;
                std::cout << "(Receiver) Amplitudes: ";
                for (int i = 0; i < omegas.size(); i++) {
                    auto omega = omegas[i];
                    int k = std::round(omega * n);
                    amp[i] = std::abs(y_[k]);
                    s |= (amp[i] > 0.05 * symbol_duration) << i;
                    std::cout << std::round(amp[i]) << ' ';
                }
                std::cout << '\n';
                return s;
            }


            SymbolType symbol_type(Symbol symbol) override {
                int bit_per_symbol = std::log2(symbol_count - 1);
                if (symbol == symbol_count - 1)
                    return SymbolType::Stop;
                else if (symbol == symbol_count - 2)
                    return SymbolType::Error;
                else
                    return SymbolType::Data;
            }

        };

        struct PhaseModem : Modem {
            

            // struct Symbol {
            //     uint8_t data;
            //     Symbol(uint8_t value) : data(value) { }
            //     operator uint8_t() { return data; }
            //     static auto
            // };
            /* e.g. 4 freqs, 3 phases

                phase 0 ~ 180
                    0           : 0
                    90          : 1
                    180         : 2

                freq
                    1047 do     data[0] = 0 / 1 / 2
                    1318 me     data[1] = 0 / 1 / 2
                    1568 so     data[2] = 0 / 1 / 2
                    9600        data[3] = 0 (reference phase)

                data = {0, 2, 1, 0}
            */

            // encode data to sin waves
            std::vector<float> omegas;
            std::vector<float> phases;

            PhaseModem(std::vector<float> omegas, std::vector<float> phases, int symbol_duration)
                : Modem(symbol_duration, std::pow(phases.size(), omegas.size())),
                omegas(omegas),
                phases(phases)
            {
                std::cout << "Set Symbol Frequncies:\n [";
                for (auto f : omegas)
                    std::cout << f << ", ";
                std::cout << "]\n";

                std::cout << "Set Symbol Phases:\n [";
                for (auto p : phases)
                    std::cout << p << ", ";
                std::cout << "]\n";
            }

            struct Config {
                float omega_min;
                float omega_max;
                int omega_count;
                int phase_count;
                int symbol_duration;
            };

            PhaseModem(Config config)
                : PhaseModem {
                [](auto omega_min, auto omega_max, auto omega_count) {
                    std::vector<float> omegas(omega_count);
                    for (int i = 0; i < omega_count; i++)
                    omegas[i] = omega_min + (omega_max - omega_min) * i / (omega_count - 1);
                    return omegas;
                } (config.omega_min, config.omega_max, config.omega_count),
                [](auto phase_count) {
                    std::vector<float> phases(phase_count);
                    for (int i = 0; i < phase_count; i++)
                    phases[i] = std::acos(1 - 2. * i / (phase_count - 1));
                    return phases;
                } (config.phase_count),
                config.symbol_duration,
                } { }

            std::function<float(int)> encode(Symbol symbol) override {
                if (symbol > symbol_count)
                    throw std::runtime_error("Error: symbol out of range");

                std::vector<std::pair<float, float>> omega_phase_pairs;
                for (int i = 0; i < omegas.size() - 1; i++) {
                    omega_phase_pairs.emplace_back(omegas[i], phases[symbol % phases.size()]);
                    symbol /= phases.size();
                }
                omega_phase_pairs.emplace_back(omegas.back(), 0);

                return [omega_phase_pairs](int i) {
                    float v = 0;
                    for (const auto &[omega, phase] : omega_phase_pairs)
                        v += std::sin(2 * std::numbers::pi * omega * i + phase);
                    v /= omega_phase_pairs.size();
                    return v;
                };
            }


            Symbol decode(std::span<float> y) override {
                auto n = y.size();

                // auto hamming = Signals::Hamming(n);
                // for (auto i = 0; i < n; i++)
                //     y[i] *= hamming(i);

                auto c = std::valarray<float>(omegas.size());
                for (int i = 0; i < omegas.size(); i++) {
                    auto omega = omegas[i];
                    c[i] = 0;
                    for (int j = 0; j < n; j++)
                        c[i] += std::sin(2 * std::numbers::pi * omega * j) * y[j];
                }
                c /= -c[c.size() - 1];
                c -= c[c.size() - 1];

                int symbol = 0;
                for (int i = c.size() - 2; i >= 0; i--) {
                    symbol *= phases.size();
                    symbol += std::round(c[i]);
                }

                return symbol;
            }



            
        };

        struct QAMModem : Modem {

            float omega;
            int order;
            std::vector<std::complex<float>> symbols;
            Signals::Butter<float> butter;

        public:

            struct Config {
                float omega;
                int duration;
                Signals::Butter<float> butter;
                int order = 2;
            };

            QAMModem(Config config)
                : QAMModem(config.omega, config.duration, config.butter, config.order) { }

            QAMModem(float omega, int duration, Signals::Butter<float> butter, int order = 2)
                : Modem(duration, order * order), omega(omega), order(order), symbols([](int order){
                    auto phase = std::vector<std::complex<float>>(order * order);
                    for (int i = 0; i < order; i++)
                        for (int j = 0; j < order; j++) {
                            phase[i * order + j] = std::complex<float> (
                                (2. * i / (order - 1) - 1),
                                (2. * j / (order - 1) - 1)
                            ) / std::sqrt(2.f);
                            // std::cout << "(" << phase[i * order + j].real() << ", " << phase[i * order + j].imag() << ") ";
                        }
                    return phase;
                }(order)), butter(butter) { }

            
            std::function<float(int)> encode(Symbol symbol) override {
                if (symbol > symbol_count)
                    throw std::runtime_error("Error: symbol out of range");
                
                return [omega=this->omega, phase=symbols[symbol]](int i) {
                    auto p = 2 * std::numbers::pi * omega * i;
                    return phase.real() * std::cos(p) + phase.imag() * std::sin(p);
                };
            }

            std::optional<int> phase_offset;


            float standard_amplitude = 1;
            Symbol decode(std::span<float> y) override {
                
                auto filtered = butter.filter(y);

                for (auto i = 0; i < y.size(); i++)
                    ofile << filtered[i] << std::endl;

                auto i_begin = y.size() / 4;
                auto i_end = y.size() * 3 / 4;
                // auto i_begin = 0;
                // auto i_end = y.size();
                i_end = std::min(i_end, filtered.size());

                int offset = 0;
                if (phase_offset)
                    offset = *phase_offset;
    
                float a = 0, b = 0;
                for (auto i = i_begin; i < i_end; i++) {
                    a += filtered[i+offset] * std::sin(2 * std::numbers::pi * omega * i);
                    b += filtered[i+offset] * std::cos(2 * std::numbers::pi * omega * i);
                }

                a /= i_end - i_begin;
                b /= i_end - i_begin;

                a /= standard_amplitude;
                b /= standard_amplitude;

                // if (phase_offset.has_value()) 
                //     std::cout << "(Receiver) Decoding: (" << a << ", " << b << ")" << std::endl;

                
                if (!phase_offset.has_value()) {
                    standard_amplitude = std::sqrt(a * a + b * b);
                    std::cout << "(Receiver) Standard amplitude: " << standard_amplitude << std::endl;
                    float phase = - std::atan2(b, a);
                    if (phase < 0)
                        phase += 2 * std::numbers::pi;
                    phase_offset = std::round(1 / (2 * std::numbers::pi * omega) * phase);
                    std::cout << "(Receiver) Set offset: " << *phase_offset << std::endl;
                    return -1;
                }


                int i, j;

                if (a < 0 && b < 0)
                    i = 0, j = 0;
                else if (a < 0 && b > 0)
                    i = 0, j = 1;
                else if (a > 0 && b < 0)
                    i = 1, j = 0;
                else if (a > 0 && b > 0)
                    i = 1, j = 1;

                // // std::cerr << "a = " << a << ", b = " << b << std::endl;
                
                // a *= std::sqrt(2);
                // b *= std::sqrt(2);
                // i = std::round((a + 1) * (order - 1) / 2);
                // j = std::round((b + 1) * (order - 1) / 2);
                
                // // if (phase_offset.has_value())
                // //     std::cout << "(Receiver) Decoding: (i=" << i << ", j=" << j << ")" << std::endl;

                return i + j * order;
            }

            
            virtual Generator<float> create_calibrate() override {
                return {[omega=this->omega](int i) { return std::sin(2 * std::numbers::pi * omega * i); }, symbol_duration, "Modem Calibrate"};
            }

            Symbol data_end_symbol() {
                return -1;
            }
            auto create_data_end() {
                return create(data_end_symbol());
            }


            std::vector<bool> symbol_to_bits(Symbol symbol) override {
                std::vector<bool> bits;
                int bit_per_symbol = std::log2(symbol_count);
                for (int i = 0; i < bit_per_symbol; i++)
                    bits.push_back((symbol >> i) & 1);
                return bits;
            }

            std::vector<Symbol> bits_to_symbols(const std::vector<bool>& bits) override {
                std::vector<Symbol> symbols;
                int bit_per_symbol = std::log2(symbol_count);
                for (int i = 0; i < bits.size(); i += bit_per_symbol) {
                    Modem::Symbol symbol = 0;
                    for (int j = 0; j < bit_per_symbol && i + j < bits.size(); j++)
                        symbol |= bits[i + j] << j;
                    symbols.push_back(symbol);
                }
                return symbols;
            }


            std::ofstream ofile { "QAM.txt" };

            float amplitude_threshold = 0;
            float calibrate_counter = 0;

            
            virtual SymbolType symbol_type(Symbol symbol) {
                if (symbol == (Symbol)-1)
                    return SymbolType::Pass;
                return SymbolType::Data;
            }




        } modem;

        
        class Sender {
            enum class State {
                Idle,
                Sending,
                Stopping,
            } state = State::Idle;

            GeneratorQueue<float> outputGenerator;
            std::mutex generator_mutex;
            Modem& modem;
            Preamble& preamble;
            std::ofstream ofile;

        public:

            Sender(Modem &modem, Preamble& preamble) : modem(modem), preamble(preamble) {
                ofile.open("sender.log");
            }

            void send(const std::vector<bool> &data) {
                std::lock_guard<std::mutex> lock(generator_mutex);
                if (state == State::Idle) {
                    outputGenerator.push(preamble.create());
                    outputGenerator.push(modem.create_calibrate());
                }
                auto symbols = modem.bits_to_symbols(data);
                for (Modem::Symbol symbol: modem.bits_to_symbols(data))
                    outputGenerator.push(modem.create(symbol));
                outputGenerator.push(modem.create_data_end());
                if (state == State::Idle) {
                    state = State::Sending;
                    // std::cout << "(Sender) Started" << std::endl;
                }
            }

            void handleCallback(DataView &p) noexcept {
                std::lock_guard<std::mutex> lock(generator_mutex);
                switch (state) {
                    case State::Idle:
                        p.zero();
                        break;
                    case State::Sending:
                        for (auto i = 0; i < p.getNumSamples(); i++) {
                            if (outputGenerator.empty()) {
                                state = State::Stopping;
                            }
                            float y = outputGenerator();
                            p(0, i) = y;
                            p(1, i) = 0;
                            ofile << y << std::endl;
                        }
                        break;
                    case State::Stopping:
                        for (auto i = 0; i < p.getNumSamples(); i++) {
                            if (outputGenerator.empty()) {
                                // if (state == State::Stopping)
                                //     std::cout << "(Sender) Stopped" << std::endl;
                                state = State::Idle;
                            }
                            float y = outputGenerator();
                            p(0, i) = y;
                            p(1, i) = 0;
                            ofile << y << std::endl;
                        }
                        break;

                }
            }
        } sender;

        class Receiver {
            enum class State {
                Calibrating,
                Idle,
                PreambleStarted,
                ModemCalibrating,
                FetchingData,
                Stop,

            } state = State::Calibrating;
            
            std::queue<Modem::Symbol> inputQueue;

            std::vector<float> inputBuffer; // the buffer to store signal for symbol decoding


            Modem &modem;
            Preamble &preamble;

            std::ofstream ofile;
            std::mutex mutex;

        public:

            Receiver(Modem &modem, Preamble &preamble) : modem(modem), preamble(preamble) {
                ofile.open("receiver.log");
            }

            bool available() {
                return !inputQueue.empty();
            }

            std::vector<bool> read() {
                std::lock_guard<std::mutex> lock(mutex);
                if (inputQueue.empty())
                    return {};
                else {
                    auto symbol_count = modem.symbol_count;
                    int bit_per_symbol = std::log2(symbol_count);
                    auto symbol_mask = (1 << bit_per_symbol) - 1;
                    std::vector<bool> data;
                    while (!inputQueue.empty()) {
                        auto symbol = inputQueue.front();
                        for (bool bits: modem.symbol_to_bits(symbol))
                            data.push_back(bits);
                        inputQueue.pop();
                    }
                    return data;
                }
            }

            void handleCallback(const DataView &p) noexcept {

                std::lock_guard<std::mutex> lock(mutex);

                for (int i = 0; i < p.getNumSamples(); i++)
                    ofile << p(0, i) << std::endl;

                
                switch (state) {
                    case State::Calibrating:
                        if (preamble.calibrate(p)) {
                            state = State::Idle;
                            // std::cout << "(Receiver) Calibration finished" << std::endl;
                        }
                        break;

                    case State::Idle:
                        if (auto start = preamble.search_preamble(p)) {
                            state = State::PreambleStarted;
                            // std::cout << "(Receiver) Preamble started at " << *start << std::endl;
                        }
                        break;
                    
                    case State::PreambleStarted:
                        if (auto stop = preamble.preamble_stop(p)) {
                            if (*stop == -1) {
                                // std::cout << "(Receiver) Preamble stopped at " << *stop << std::endl;
                                state = State::Idle;
                            } else {
                                // std::cout << "(Receiver) Preamble stopped at " << *stop << std::endl;
                                state = State::FetchingData;
                                inputBuffer.clear();
                                for (int i = *stop; i < std::min(
                                    modem.symbol_duration - 
                                    int(inputBuffer.size()),
                                    int(p.getNumSamples())
                                ); i++) {
                                    inputBuffer.push_back(p(0, i));
                                }
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
                                    // std::cerr << "(Receiver) Error: inputBuffer overflow" << std::endl;
                                    exit(-1);
                                } else if (inputBuffer.size() == modem.symbol_duration) {
                                    Modem::Symbol symbol = modem.decode(inputBuffer);
                                    // handle symbol 
                                    switch (modem.symbol_type(symbol)) {
                                        case Modem::SymbolType::Data:
                                            std::cout << "(Receiver) Data symbol: " << (int)symbol << std::endl;
                                            inputQueue.push(symbol);
                                            break;
                                        case Modem::SymbolType::Stop:
                                            // std::cout << "(Receiver) Stop symbol " << (int)symbol << std::endl;
                                            state = State::Stop;
                                            break;
                                        case Modem::SymbolType::Error:
                                            // std::cerr << "(Receiver) Error symbol: " << (int)symbol << std::endl;
                                            break;
                                        case Modem::SymbolType::Pass:
                                            std::cout << "(Receiver) Pass symbol" << std::endl;
                                            break;
                                    }

                                    inputBuffer.clear();

                                    i_start = i_max;
                                    
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

        } receiver;

        bool running = false;

    public:
        BitStreamDeviceIOHandler(BitStreamDeviceConfig c = { }) :
            config(c),
            // modem { {
            //     .omega_min = 440,
            //     .omega_max = 1000,
            //     .omega_count = 3,
            //     .phase_count = 3,
            //     .symbol_duration = 12000,
            // } },
            // modem {
            //     {1047, 1318, 1568, 2093, 9600},
            //     {0, std::numbers::pi},
            //     12000,
            // },
            // modem {
            //      {{2000.,  4209., 5100, 6100 }},
            //     //  {{4000.,  5000 , 6000.,}},
            //     //  {{512.,  1024.,  2048.,  4096., 8192.}},
            //      8192,
            // },
            modem { {
                .omega = 2400. / 48000.,
                .duration = 1000,
                .butter = {
                    { 0.00016822370859146914, 0.0, -0.0003364474171829383, 0.0, 0.00016822370859146914},
                    { 1.0, -3.76934096654668, 5.515261708905102, -3.7001989105751507, 0.9636529842237052}
                },
                .order = 2,
            } },
            preamble { {
                .omega = 8000. / 48000.,
                .duration = 1000,
                .butter = {
                    {0.00016822,  0.        , -0.00033645,  0.        ,  0.00016822},
                    {1.        , -1.98165982,  2.94480658, -1.94530969,  0.96365298}
                }
            } },
            sender { modem, preamble },
            receiver { modem, preamble } 
        { 
            std::cout << "Initializing device" << std::endl;
        }


        void inputCallback(const DataView &p) noexcept override { receiver.handleCallback(p); }
        void outputCallback(DataView &p) noexcept override { sender.handleCallback(p); }
        void send(const auto& data) { sender.send(data); }
        bool available() { return receiver.available(); }
        auto read() { return receiver.read(); }

    };

    struct BitStreamDevice {

        std::shared_ptr<BitStreamDeviceIOHandler> io;
        std::shared_ptr<Device> device;

        BitStreamDevice() {
            device = std::make_shared<Device>();
            device->open();
            device->start(io = std::make_shared<BitStreamDeviceIOHandler>());
        }

        ~BitStreamDevice() {
            device->stop();
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

    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::cout << "Start Transmit" << std::endl;

    // //3       6      3      4
    // Project1::sout << std::vector<bool> {
    //     0,0, 1,0, 0,1, 1,1, 
    //     0,0, 1,0, 0,1, 1,1, 
    //     0,0, 1,0, 0,1, 1,1, 
    //     0,0, 1,0, 0,1, 1,1, 
    //     0,0, 1,0, 0,1, 1,1, 
    //     // 0,1, 0,0,
    //     // 1,1, 0,0, 0,0, 0,1,
    //     // 0,0, 1,1
    // };
    // std::this_thread::sleep_for(std::chrono::seconds(1));

    std::vector<bool> input, output;
    std::ifstream ifile { "INPUT.txt" };
    std::ofstream ofile { "OUTPUT.txt" };

    bool bit;
    for (int i = 0; i < 10000; i++) {
        ifile >> bit;
        input.push_back(bit);
    }

    Project1::sout << input;
    
    std::this_thread::sleep_for(std::chrono::seconds(15));

    Project1::sin >> output;
    for (auto bit : output)
        ofile << bit << std::endl;

    // for (int i = 0; i < 10000; i++) {
    //     ofile << output[i] << std::endl;
    // }

    return 0;

}

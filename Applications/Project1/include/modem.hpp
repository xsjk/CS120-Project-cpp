#pragma once

#include <functional>
#include <vector>
#include <span>
#include <optional>
#include <fstream>
#include <bitset>
#include <cmath>

#include "generator.hpp"

namespace Physical {


struct Modem {

    using Symbol = uint8_t;

    const int symbol_duration;
    const int symbol_count;

    Modem(int symbol_duration, int symbol_count)
        : symbol_duration(symbol_duration), symbol_count(symbol_count) { }

    virtual std::function<float(int)> encode(Symbol symbol) = 0;
    virtual Symbol decode(std::span<float>) = 0;

    Generator<float> create(Symbol symbol) {
        // std::cout << "(Sender) Create symbol " << (int)symbol << '\n';
        return {encode(symbol), symbol_duration, "Symbol " + std::to_string(symbol)};
    }

    virtual std::vector<bool> symbol_to_bits(Symbol symbol) = 0;
    virtual std::vector<Symbol> bits_to_symbols(const std::vector<bool>& bits) = 0;

    virtual Generator<float> create_calibrate() = 0;

    Symbol data_end_symbol() {
        return symbol_count - 1;
    }
    Generator<float> create_data_end() {
        // stop for two symbol_duration
        return {[](int){return 0;}, symbol_duration * 3, "Data End"};
    }

    enum class SymbolType {
        Data,
        Stop,
        Error,
        Pass,
    };
    
    virtual SymbolType symbol_type(Symbol symbol) {
        if (symbol == (Symbol)-1)
            return SymbolType::Pass;
        else if (symbol == (Symbol)-2)
            return SymbolType::Stop;
        return SymbolType::Data;
    }

    virtual void reset() {

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

class SimpleModem : public Modem {

    int carrierSize;

    std::vector<float> carrier_sin; 
    std::vector<float> carrier_cos;
    #ifdef LOG
        std::ofstream ofile { "SimpleModem.txt" };
    #endif
public:
    SimpleModem(int carrierSize)
        : Modem(carrierSize, 2), carrierSize(carrierSize)
    { 
        carrier_sin.reserve(carrierSize);
        carrier_cos.reserve(carrierSize);
        for (int i = 0; i < carrierSize; i++) {
            carrier_sin.push_back(std::sin(2 * std::numbers::pi * 2345 * i / (carrierSize - 1)));
            carrier_cos.push_back(std::cos(2 * std::numbers::pi * 2345 * i / (carrierSize - 1)));
        }
    }

    std::function<float(int)> encode(Symbol symbol) override {
        if (symbol == 0)
            return [&](int i) { return carrier_sin[i]; };
        else
            return [&](int i) { return -carrier_sin[i]; };
    }

    std::optional<int> k;
    Symbol decode(std::span<float> data) override {
        float sum = 0;
        for (auto i = 0; i < data.size(); i++) {
            #ifdef LOG
                ofile << data[i] << '\n';
            #endif
            sum += data[i] * carrier_sin[i];
        }

        if (!k) {
            k = sum > 0 ? 1 : -1;
            std::cout << "(Receiver) Set k: " << *k << '\n';
            return -1;
        }
        if (sum * *k > 0.01)
            return 0;
        else if (sum * *k < -0.01)
            return 1;
        else
            return -2;
    }

    std::vector<bool> symbol_to_bits(Symbol symbol) override {
        std::vector<bool> bits;
        bits.push_back(symbol);
        return bits;
    }
    std::vector<Symbol> bits_to_symbols(const std::vector<bool>& bits) override {
        std::vector<Symbol> symbols(bits.size());
        for (int i = 0; i < bits.size(); i++)
            symbols[i] = bits[i];
        return symbols;
    }

    Generator<float> create_calibrate() override {
        return create(0);
    }

    void reset() override {
        k = {};
    }

};

struct QAMModem : Modem {

    float omega;
    int order;
    std::vector<std::complex<float>> symbols;
    Signals::Butter<float> butter;

    #ifdef LOG
        std::ofstream ofile { "QAM.txt" };
    #endif

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
            if (order > 1) {
                auto phase = std::vector<std::complex<float>>(order * order);
                for (int i = 0; i < order; i++)
                    for (int j = 0; j < order; j++) {
                        phase[i * order + j] = std::complex<float> (
                            (2. * j / (order - 1) - 1),
                            (2. * i / (order - 1) - 1)
                        ) / std::sqrt(2.f);
                        std::cout << i * order + j << " (" << phase[i * order + j].real() << ", " << phase[i * order + j].imag() << ")\n";
                    }
                return phase;
            } else {
                return std::vector<std::complex<float>> { 1, -1 };
            }
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
        // auto filtered = y;

        #ifdef LOG
            for (auto i = 0; i < filtered.size(); i++)
                ofile << filtered[i] << '\n';
        #endif

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
            a += filtered[i+offset] * std::cos(2 * std::numbers::pi * omega * i);
            b += filtered[i+offset] * std::sin(2 * std::numbers::pi * omega * i);
        }

        a /= i_end - i_begin;
        b /= i_end - i_begin;

        if (!phase_offset.has_value()) {
            standard_amplitude = std::sqrt(a * a + b * b);
            // std::cout << "(Receiver) Standard amplitude: " << standard_amplitude << '\n';
            auto phase = std::atan2(b, a);
            if (phase < 0)
                phase += 2 * std::numbers::pi;
            phase_offset = std::round(phase / (2 * std::numbers::pi * omega));
            // std::cout << "(Receiver) Set offset: " << *phase_offset << '\n';
            return -1;
        }

        a /= standard_amplitude;
        b /= standard_amplitude;

        // if (phase_offset.has_value()) 
        //     std::cout << "(Receiver) Decoding: (" << a << ", " << b << ")" << '\n';

        int i, j;

        if (order==1) {
            return a < 0;
        }
        else if (order==2) {
            if (a < 0 && b < 0)
                i = 0, j = 0;
            else if (a < 0 && b > 0)
                i = 0, j = 1;
            else if (a > 0 && b < 0)
                i = 1, j = 0;
            else if (a > 0 && b > 0)
                i = 1, j = 1;
        } else {
            a *= std::sqrt(2);
            b *= std::sqrt(2);
            i = std::round((a + 1) * (order - 1) / 2);
            j = std::round((b + 1) * (order - 1) / 2);
        }

        return i + j * order;
    }

    
    virtual Generator<float> create_calibrate() override {
        return {[omega=this->omega](int i) { return std::cos(2 * std::numbers::pi * omega * i); }, symbol_duration, "Modem Calibrate"};
    }


    std::vector<bool> symbol_to_bits(Symbol symbol) override {
        std::vector<bool> bits;
        if (order == 1) {
            bits.push_back(symbol);
            return bits;
        }
        int bit_per_symbol = std::log2(symbol_count);
        for (int i = 0; i < bit_per_symbol; i++)
            bits.push_back((symbol >> i) & 1);
        return bits;
    }

    std::vector<Symbol> bits_to_symbols(const std::vector<bool>& bits) override {
        std::vector<Symbol> symbols;
        if (order == 1) {
            symbols.reserve(bits.size());
            for (auto b: bits) 
                symbols.push_back(b);
            return symbols;
        }
        int bit_per_symbol = std::log2(symbol_count);
        for (int i = 0; i < bits.size(); i += bit_per_symbol) {
            Modem::Symbol symbol = 0;
            for (int j = 0; j < bit_per_symbol && i + j < bits.size(); j++)
                symbol |= bits[i + j] << j;
            symbols.push_back(symbol);
        }
        return symbols;
    }


    float amplitude_threshold = 0;
    float calibrate_counter = 0;

    void reset() override {
        phase_offset = {};
    }

};

struct DigitalModem : Modem {

    // no modulation, just send bits
    DigitalModem(int symbol_duration) : Modem(symbol_duration, 3) { }
    std::function<float(int)> encode(Symbol symbol) override {
        if (symbol == 0)
            return [](int i) { return 1; };
        else
            return [](int i) { return -1; };
    }
    Symbol decode(std::span<float> y) override {
        float sum = 0;
        for (auto i = 0; i < y.size(); i++)
            sum += y[i];
        return sum > 0 ? 0 : 1;
    }
    std::vector<bool> symbol_to_bits(Symbol symbol) override {
        std::vector<bool> bits;
        bits.push_back(symbol);
        return bits;
    }
    std::vector<Symbol> bits_to_symbols(const std::vector<bool>& bits) override {
        std::vector<Symbol> symbols(bits.size());
        for (int i = 0; i < bits.size(); i++)
            symbols[i] = bits[i];
        return symbols;
    }
    
};



}
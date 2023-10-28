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

#include "signal.hpp"
#include "wasapidevice.hpp"

using namespace WASAPI;



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
        std::function<T(float)> func;
        float fs;
        int ticks;
        int current_tick = 0;
    public:
        std::string name;
        Generator(std::function<T(float)> func, float fs, int ticks, std::string name = "")
            : func(std::move(func)), fs(fs), ticks(ticks), name(std::move(name)) { }

        const Generator& operator=(const Generator&) = delete;

        T next() { 
            if (current_tick == 0) 
                std::cout << "(Sender) " << name << " started" << std::endl;
            current_tick++;
            return func(current_tick / fs); 
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

        T next() {
            if (queue.empty())
                return 0;
            auto& gen = queue.front();
            if (gen.empty()) {
                pop();
                // std::cout << "(Sender) " << gen.name << "finished" << std::endl;
                return next();
            }
            ticks -= 1;
            return gen.next();
        }

        T operator()() { return next(); }
    };


    class BitStreamDeviceIOHandler;
    class BitStreamDeviceIOHandler : public std::enable_shared_from_this<BitStreamDeviceIOHandler>, public IOHandler {

        struct BitStreamDeviceConfig {
            float fs;
        } config;


        struct Preamble {
            virtual Generator<float> create() noexcept = 0;
            virtual bool calibrate(const DataView& p) noexcept = 0;
            virtual std::optional<int> search_preamble(const DataView& p) noexcept = 0;
            virtual std::optional<int> preamble_stop(const DataView& p) noexcept = 0;
        };

        struct SinePreamble : Preamble {
            float freq;
            int duration;
            float fs;
            Signals::Butter<float> butter;
            std::vector<float> buffer;

            std::ofstream ofile { "out.txt" };


            struct Config {
                float freq;
                int duration;
                float fs;
                Signals::Butter<float> butter;
            };

            SinePreamble(Config config) : freq(config.freq), duration(config.duration), fs(config.fs), butter(config.butter) { }

            Generator<float> create() noexcept override {
                return Generator<float>(
                    [freq=this->freq](float t) { return std::sin(2 * std::numbers::pi * freq * t); },
                    fs,
                    duration,
                    "Preamble"
                );
            }


            // continuous 5 chunks of data with same amplitude
            float amplitude_threshold = 0;
            float calibrate_counter = 0;
            bool calibrate(const DataView& p) noexcept {
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
                    amplitude_threshold *= 10;
                    std::cout << "(Receiver) Amplitude threshold: " << amplitude_threshold << std::endl;
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
                        frame_from_preamble = duration - i;
                }
                if (preamble_start_frame)
                    butter.clean();
                return preamble_start_frame;
            }

            std::optional<int> preamble_stop(const DataView& p) noexcept override {
                auto filtered = butter.filter(p[0]);
                if (frame_from_preamble + p.getNumSamples() < duration)
                    return {};
                else 
                    return frame_from_preamble + p.getNumSamples() - duration;
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
                return preamble_end_frame;
            } 

        } preamble;


        struct ChirpPreamble : Preamble {
            /// NOT IMPLEMENTED
        };


        struct Modem {

            using Symbol = uint8_t;

            const int symbol_duration;
            const int symbol_count;
            const float fs;

            Modem(int symbol_duration, int symbol_count, float fs)
                : symbol_duration(symbol_duration), symbol_count(symbol_count), fs(fs) { }

            virtual std::function<float(float)> encode(Symbol symbol) = 0;
            virtual Symbol decode(const std::vector<float> &y) = 0;

            Generator<float> create(Symbol symbol) {
                return {encode(symbol), fs, symbol_duration, "Symbol " + std::to_string(symbol)};
            }
            // auto create(const std::vector<Symbol>& symbols) {
            //     std::vector<Generator<float>> generators;
            //     for (auto symbol : symbols)
            //         generators.push_back(create(symbol));
            //     return generators;
            // }
            // virtual std::vector<Generator<float>> create(const std::vector<bool>& data) = 0;

            virtual std::vector<bool> symbol_to_bits(Symbol symbol) = 0;
            virtual std::vector<Symbol> bits_to_symbols(const std::vector<bool>& bits) = 0;


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
            };
            
            virtual SymbolType symbol_type(Symbol symbol) {
                if (symbol == symbol_count - 1)
                    return SymbolType::Stop;
                else if (symbol > symbol_count - 2)
                    return SymbolType::Error;
                else
                    return SymbolType::Data;
            }

            

        };

        class FreqModem : public Modem {

            std::vector<float> freqs;
        
        public:
            
            FreqModem(std::vector<float> freqs, int symbol_duration, float fs)
                : Modem(symbol_duration, 1 << freqs.size(), fs), freqs(freqs)
            {
                std::cout << "(Receiver) Set Symbol Frequncies: [";
                for (auto f : freqs)
                    std::cout << f << ", ";
                std::cout << "\b\b]\n";

                ofile.open("fft.log");
            }

            struct Config {
                float freq_min;
                float freq_max;
                int freq_count;
                int symbol_duration;
                float fs;
            };

            std::ofstream ofile;

            FreqModem(Config config)
                : FreqModem {
                [](auto freq_min, auto freq_max, auto freq_count) {
                    std::vector<float> freqs(freq_count);
                    for (int i = 0; i < freq_count; i++)
                    freqs[i] = freq_min + (freq_max - freq_min) * i / (freq_count - 1);
                    return freqs;
                } (config.freq_min, config.freq_max, config.freq_count),
                config.symbol_duration,
                config.fs
                } { }
            
            // std::vector<Generator<float>> create(const std::vector<bool>& data) override {
            //     std::vector<Generator<float>> funcs;
            //     int bit_per_symbol = std::log2(symbol_count - 1);
            //     for (int i = 0; i < data.size(); i += bit_per_symbol) {
            //         Modem::Symbol symbol = 0;
            //         for (int j = 0; j < bit_per_symbol; j++)
            //             symbol |= data[i + j] << j;
            //         funcs.emplace_back(encode(symbol), fs, symbol_duration, "Symbol " + std::to_string(symbol));
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

            std::function<float(float)> encode(Symbol symbol) {
                if (symbol > symbol_count)
                    throw std::runtime_error("Error: symbol out of range");
                
                return [symbol, freqs=this->freqs](float t) {
                    float v = 0;
                    for (int i = 0; i < freqs.size(); i++)
                        if (symbol & (1 << i))
                            v += std::sin(2 * std::numbers::pi * freqs[i] * t);
                    // v += std::sin(2 * std::numbers::pi * freqs[freqs.size()-1] * t);
                    return v / freqs.size();
                };
            }


            Symbol decode(const std::vector<float> &y) {
                auto n = y.size();
                std::vector<float> y_ = y;
                Signals::fft<float>(y_);

                for (auto i : y_)
                    ofile << i << ' ';
            
                ofile << '\n';
                std::vector<float> amp(freqs.size());
                /// iterate over freqs and get amplitude
                Symbol s = 0;
                // std::cout << "(Receiver) Amplitudes: ";
                for (int i = 0; i < freqs.size(); i++) {
                    auto freq = freqs[i];
                    int k = std::round(freq * n / fs);
                    amp[i] = std::abs(y_[k]);
                    s |= (amp[i] > 0.05 * symbol_duration) << i;
                    // std::cout << std::round(amp[i]) << ' ';
                }
                // std::cout << '\n';
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

        } modem;


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
            std::vector<float> freqs;
            std::vector<float> phases;

            PhaseModem(std::vector<float> freqs, std::vector<float> phases, int symbol_duration, float fs)
                : Modem(symbol_duration, std::pow(phases.size(), freqs.size()), fs),
                freqs(freqs),
                phases(phases)
            {
                std::cout << "Set Symbol Frequncies:\n [";
                for (auto f : freqs)
                    std::cout << f << ", ";
                std::cout << "]\n";

                std::cout << "Set Symbol Phases:\n [";
                for (auto p : phases)
                    std::cout << p << ", ";
                std::cout << "]\n";
            }

            struct Config {
                float freq_min;
                float freq_max;
                int freq_count;
                int phase_count;
                int symbol_duration;
                float fs;
            };

            PhaseModem(Config config)
                : PhaseModem {
                [](auto freq_min, auto freq_max, auto freq_count) {
                    std::vector<float> freqs(freq_count);
                    for (int i = 0; i < freq_count; i++)
                    freqs[i] = freq_min + (freq_max - freq_min) * i / (freq_count - 1);
                    return freqs;
                } (config.freq_min, config.freq_max, config.freq_count),
                [](auto phase_count) {
                    std::vector<float> phases(phase_count);
                    for (int i = 0; i < phase_count; i++)
                    phases[i] = std::acos(1 - 2. * i / (phase_count - 1));
                    return phases;
                } (config.phase_count),
                config.symbol_duration,
                config.fs
                } { }

            std::function<float(float)> encode(Symbol symbol) {
                if (symbol > symbol_count)
                    throw std::runtime_error("Error: symbol out of range");

                std::vector<std::pair<float, float>> freq_phase_pairs;
                for (int i = 0; i < freqs.size() - 1; i++) {
                    freq_phase_pairs.emplace_back(freqs[i], phases[symbol % phases.size()]);
                    symbol /= phases.size();
                }
                freq_phase_pairs.emplace_back(freqs.back(), 0);

                return [freq_phase_pairs](float t) {
                    float v = 0;
                    for (const auto &[freq, phase] : freq_phase_pairs)
                        v += std::sin(2 * std::numbers::pi * freq * t + phase);
                    v /= freq_phase_pairs.size();
                    return v;
                };
            }


            Symbol decode(const std::vector<float> &y) {
                auto n = y.size();

                // auto hamming = Signals::Hamming(n);
                // for (auto i = 0; i < n; i++)
                //     y[i] *= hamming(i);

                auto c = std::valarray<float>(freqs.size());
                for (int i = 0; i < freqs.size(); i++) {
                    auto freq = freqs[i];
                    c[i] = 0;
                    for (int j = 0; j < n; j++)
                        c[i] += std::sin(2 * std::numbers::pi * freq * j / fs) * y[j];
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


        class Sender {
            enum class State {
                Idle,
                Sending,
                Stopping,
            } state = State::Idle;

            GeneratorQueue<float> outputGenerator;
            Modem& modem;
            Preamble& preamble;
            std::ofstream ofile;

        public:

            Sender(Modem &modem, Preamble& preamble) : modem(modem), preamble(preamble) {
                ofile.open("sender.log");
            }

            void send(const std::vector<bool> &data) {

                if (state == State::Idle) {
                    state = State::Sending;
                    outputGenerator.push(preamble.create());
                }
                auto symbols = modem.bits_to_symbols(data);
                for (Modem::Symbol symbol: modem.bits_to_symbols(data))
                    outputGenerator.push(modem.create(symbol));
                outputGenerator.push(modem.create_data_end());

            }

            void handleCallback(DataView &p) noexcept {
                switch (state) {
                    case State::Idle:
                        p.zero();
                        break;
                    case State::Sending:
                        for (auto i = 0; i < p.getNumSamples(); i++) {
                            if (outputGenerator.empty()) {
                                outputGenerator.push(modem.create(modem.symbol_count));
                                state = State::Stopping;
                            }
                            float y = outputGenerator();
                            p(0, i) = p(1, i) = y;
                            ofile << y << std::endl;
                        }
                        break;
                    case State::Stopping:
                        for (auto i = 0; i < p.getNumSamples(); i++) {
                            if (outputGenerator.empty()) {
                                if (state == State::Stopping)
                                    std::cout << "(Sender) Stopped" << std::endl;
                                state = State::Idle;
                            }
                            float y = outputGenerator();
                            p(0, i) = p(1, i) = y;
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
                FetchingData,
                Stop,

            } state = State::Calibrating;
            
            std::queue<Modem::Symbol> inputQueue;

            std::vector<float> inputBuffer; // the buffer to store signal for symbol decoding


            Modem &modem;
            Preamble &preamble;

            std::ofstream ofile;

        public:

            Receiver(Modem &modem, Preamble &preamble) : modem(modem), preamble(preamble) {
                ofile.open("receiver.log");
            }

            bool available() {
                return !inputQueue.empty();
            }

            std::vector<bool> read() {
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

                for (int i = 0; i < p.getNumSamples(); i++)
                    ofile << p(0, i) << std::endl;

                switch (state) {
                    case State::Calibrating:
                        if (preamble.calibrate(p)) {
                            state = State::Idle;
                            std::cout << "(Receiver) Calibration finished" << std::endl;
                        }
                        break;

                    case State::Idle:
                        if (auto start = preamble.search_preamble(p)) {
                            state = State::PreambleStarted;
                            std::cout << "(Receiver) Preamble started at " << *start << std::endl;
                        }
                        break;
                    
                    case State::PreambleStarted:
                        if (auto stop = preamble.preamble_stop(p)) {
                            state = State::FetchingData;
                            std::cout << "(Receiver) Preamble stopped at " << *stop << std::endl;
                            inputBuffer.clear();
                            for (int i = *stop; i < inputBuffer.size(); i++) {
                                inputBuffer.push_back(p(0, i));
                            }
                        }
                        break;
                    case State::FetchingData:
                        {
                            auto i = 0;
                            auto i_max = std::min(
                                modem.symbol_duration - 
                                int(inputBuffer.size()),
                                int(p.getNumSamples())
                            );
                            for (; i < i_max; i++)
                                inputBuffer.push_back(p(0, i));

                            if (inputBuffer.size() > modem.symbol_duration) {
                                std::cerr << "(Receiver) Error: inputBuffer overflow" << std::endl;
                                exit(-1);
                            }

                            if (inputBuffer.size() == modem.symbol_duration) {
                                Modem::Symbol symbol = modem.decode(inputBuffer);
                                // handle symbol 
                                switch (modem.symbol_type(symbol)) {
                                    case Modem::SymbolType::Data:
                                        std::cout << "(Receiver) Data symbol: " << (int)symbol << std::endl;
                                        inputQueue.push(symbol);
                                        break;
                                    case Modem::SymbolType::Stop:
                                        std::cout << "(Receiver) Stop symbol " << (int)symbol << std::endl;
                                        state = State::Stop;
                                        break;
                                    case Modem::SymbolType::Error:
                                        std::cerr << "(Receiver) Error symbol: " << (int)symbol << std::endl;
                                        break;
                                }

                                inputBuffer.clear();
                                for (; i < p.getNumSamples(); i++)
                                    inputBuffer.push_back(p(0, i));
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
        BitStreamDeviceIOHandler(BitStreamDeviceConfig c = { .fs = 48000 }) :
            config(c),
            // modem { {
            //     .freq_min = 440,
            //     .freq_max = 1000,
            //     .freq_count = 3,
            //     .phase_count = 3,
            //     .symbol_duration = 12000,
            //     .fs = c.fs
            // } },
            // modem {
            //     {1047, 1318, 1568, 2093, 9600},
            //     {0, std::numbers::pi},
            //     12000,
            //     c.fs
            // },
            modem {
                 {{660., 2109,  2607., }},
                //  {{4000.,  5000 , 6000.,}},
                //  {{512.,  1024.,  2048.,  4096., 8192.}},
                 4096,
                 48000,
            },
            preamble { {
                .freq = 10000,
                .duration = 1024,
                .fs = c.fs,
                .butter = Signals::Butter(
                    {0.0009, 0, -0.0019, 0, 0.0009},
                    {1.0000, -0.0000, 1.9112, 0, 0.9150}
                )
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
        Device device;

        BitStreamDevice() {
            io = std::make_shared<BitStreamDeviceIOHandler>();
            device.open();
            device.start(io);
        }

        ~BitStreamDevice() {
            // device.stop();
            std::cout << "Exit" << std::endl;
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

#include <iterator>


int main() {

    std::this_thread::sleep_for(std::chrono::seconds(1));
                                        //3       6      3      4
    // Project1::sout << std::vector<bool> {1,1, 1,0, 1,1, 0,0, 1,1, 0,0};

    std::vector<bool> input, output;
    std::ifstream ifile { "INPUT.txt" };
    bool bit;
    while (ifile >> bit)
        input.push_back(bit);
    Project1::sout << input;
    std::cout << "Input Size: " << input.size() << std::endl;
    

    std::this_thread::sleep_for(std::chrono::seconds(20));

    Project1::sin >> output;
    std::copy(output.begin(), output.end(), std::ostream_iterator<bool>(std::cout, " "));

    return 0;

}

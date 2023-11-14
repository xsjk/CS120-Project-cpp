#pragma once

#include "generator.hpp"
#include <optional>
#include <fstream>
#include <vector>
#include <deque>
#include <numeric>
#include <ranges>

#include "utils.hpp"
#include "device.hpp"

namespace Physical {

struct Preamble {
    virtual Generator<float> create() noexcept = 0;
    virtual bool calibrate(const DataView<float> &p) noexcept { return true; };
    virtual std::optional<int> wait(const DataView<float> &p) noexcept = 0;
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
    bool calibrate(const DataView<float> &p) noexcept override {
        if (calibrate_counter++ < 100) {
            auto filtered = butter.filter(p[0]);
            float max_amplitude = 0;
            for (int i = 0; i < filtered.size(); i++) {
                max_amplitude = std::max(max_amplitude, std::abs(filtered[i])); 
                ofile << filtered[i] << '\n';
            }
            amplitude_threshold = std::max(amplitude_threshold, max_amplitude);
            return false;
        } else {
            amplitude_threshold *= 30;
            std::cout << "(Receiver) Preamble Amplitude threshold: " << amplitude_threshold << '\n';
            return true;
        }
    }


    std::optional<int> preamble_end_frame;
    std::optional<int> wait(const DataView<float> &p) noexcept override {
        auto filtered = butter.filter(p[0]);
        if (!preamble_end_frame)
            for (auto i = 0; i < filtered.size(); i++) {
                ofile << filtered[i] << '\n';
                if (!preamble_end_frame && std::abs(filtered[i]) > amplitude_threshold) {
                    preamble_end_frame = i + duration;
                    break;
                }
            }
        if (preamble_end_frame) {
            if (*preamble_end_frame < p.getNumSamples()) {
                int end = *preamble_end_frame;
                preamble_end_frame = {};
                butter.clean();
                return end;
            } else {
                preamble_end_frame = *preamble_end_frame - p.getNumSamples();
            }
        }
        return {};
    }

};

struct StaticPreamble : Preamble {
    std::vector<float> signal;
    float threshold;
    StaticPreamble(std::string path, float threshold) : signal([](std::string path) {
        std::ifstream file {path};
        std::vector<float> signal;
        float sample;
        while (file >> sample)
            signal.push_back(sample);
        return signal;
    }(path)),  threshold(threshold) { }
    Generator<float> create() noexcept override {
        return Generator<float>(
            [&](int i) { return signal[i]; },
            signal.size(),
            "Preamble"
        );
    }

    std::deque<float> buffer;

    // continuous 5 chunks of data with same amplitude
    float amplitude_threshold = 0;
    float calibrate_counter = 0;
    bool calibrate(const DataView<float> &p) noexcept override {
        if (calibrate_counter++ < 100) {
            for (auto i = 0; i < p.size(); i++) {
                buffer.emplace_back(p(0, i));
                if (buffer.size() < signal.size())
                    continue;
                float sum = 0;
                for (auto j = 0; j < signal.size(); j++)
                    sum += buffer[j] * signal[j];
                sum /= signal.size();
                buffer.pop_front();
                amplitude_threshold = std::max(amplitude_threshold, std::abs(sum));
            }
            return false;
        } else {
            amplitude_threshold *= threshold;
            std::cout << "(Receiver) Preamble Amplitude threshold: " << amplitude_threshold << '\n';
            return true;
        }
    }


    int lastBigSumI = 0;
    std::optional<int> wait(const DataView<float> &p) noexcept override {
        
        for (auto i = 0; i < p.size(); i++) {
            buffer.emplace_back(p(0, i));
            if (buffer.size() < signal.size())
                continue;
            float sum = 0;
            for (auto j = 0; j < signal.size(); j++)
                sum += buffer[j] * signal[j];
            sum /= signal.size();
            buffer.pop_front();

            // TODO: how to extract the local maxima
            if (sum > amplitude_threshold) {
                // std::cout << sum << std::endl;
                lastBigSumI = i;
                buffer.clear();

                return i;
            }
        }

        return {};
    }
    
};


}
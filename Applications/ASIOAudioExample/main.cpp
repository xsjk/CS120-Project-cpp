#include <iostream>
#include <cmath>
#include <string>
#include <numbers>
#include <fstream>
#include <memory>
#include <thread>
#include "asiosys.h"
#include "asiodrvr.h"
#include "asio.h"
#include "asiodrivers.h"
#include "asiodevice.h"


class SineWave : public ASIO::AudioCallbackHandler {
    float phase = 0;
public:
    void audioDeviceIOCallback(const float *const *inputChannelData, int numInputChannels,
                               float *const *outputChannelData, int numOutputChannels,
                               int numSamples) override {
        constexpr float dphase = 2. * std::numbers::pi * 440. / 44100;
        for (int j = 0; j < numSamples; ++j) {
            float y = std::sin(phase += dphase) / 5;
            if (phase > 2 * std::numbers::pi)
                phase -= 2 * std::numbers::pi;
            for (int i = 0; i < numOutputChannels && i < numInputChannels; ++i)
                outputChannelData[i][j] = y;
        }
    }

};


class Recorder : public ASIO::AudioCallbackHandler {
    std::ofstream f;
public:
    Recorder(std::string output = "recorded.txt") : f(output) { }
    std::vector<float> recorded;
    void audioDeviceIOCallback(const float *const *inputChannelData, int numInputChannels,
                               float *const *outputChannelData, int numOutputChannels,
                               int numSamples) override {
        for (int j = 0; j < numSamples; ++j) {
            f << inputChannelData[0][j] << '\n';
            for (int i = 0; i < numOutputChannels && i < numInputChannels; ++i) {
                outputChannelData[i][j] = inputChannelData[i][j];
            }
        }
    }
};


int main() {
    ASIO::AudioDevice asio;
    asio.open(2, 2, 44100);
    auto sinewave = std::make_shared<SineWave>();
    asio.start(std::make_shared<Recorder>());
    for (int i = 0; i < 10; ++i) {
        std::cout << i << std::endl;
        if (i % 2)
            asio.stop(sinewave);
        else
            asio.start(sinewave);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    asio.close();
    return 0;
}

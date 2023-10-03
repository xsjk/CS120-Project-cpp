#include <iostream>
#include <cmath>
#include <string>
#include <numbers>
#include <fstream>
#include "asiosys.h"
#include "asiodrvr.h"
#include "asio.h"
#include "asiodrivers.h"
#include "asiodevice.h"

class SineWave : public AudioCallbackHandler {
    float phase = 0;
public:
    void audioDeviceIOCallback(const float *const *inputChannelData, int numInputChannels,
                               float **outputChannelData, int numOutputChannels,
                               int numSamples) override {
        constexpr float dphase = 2. * std::numbers::pi * 440. / 44100;
        for (int j = 0; j < numSamples; ++j) {
            auto y = std::sinf(phase += dphase);
            if (phase > 2 * std::numbers::pi)
                phase -= 2 * std::numbers::pi;
            for (int i = 0; i < numOutputChannels && i < numInputChannels; ++i)
                outputChannelData[i][j] = y;
        }
    }

};


class Recorder : public AudioCallbackHandler {
    std::ofstream f;
public:
    Recorder(std::string output="recorded.txt") : f(output) {}
    std::vector<float> recorded;
    void audioDeviceIOCallback(const float *const *inputChannelData, int numInputChannels,
                               float **outputChannelData, int numOutputChannels,
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
    ASIODevice asio;
    asio.open(2, 2, 44100);
    asio.start(std::make_shared<SineWave>());
    std::this_thread::sleep_for(std::chrono::seconds(2));
    asio.stop();
    asio.start(std::make_shared<Recorder>());
    std::this_thread::sleep_for(std::chrono::seconds(2));
    asio.stop();
    asio.close();
    return 0;
}
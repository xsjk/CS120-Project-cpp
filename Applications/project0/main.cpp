#include "argparse/argparse.hpp"
#include "boost/json/parse.hpp"

#include <fstream>
#include <iostream>
#include <cmath>
#include <string>
#include <numbers>
#include <thread>
#include "asiosys.h"
#include "asiodrvr.h"
#include "asio.h"
#include "asiodrivers.h"
#include "asiodevice.h"


template<typename T>
auto read_binary(const std::string &filename) {
    std::vector<T> data;
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open file\n";
        return data;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size % sizeof(T) != 0) {
        std::cerr << "File size is not a multiple of element size\n";
        return data;
    }

    data.resize(size / sizeof(T));
    if (!file.read((char*)(data.data()), size)) {
        std::cerr << "Failed to read file\n";
        data.clear();
    }

    file.close();
    return data;
}

struct RecordCallback : public ASIO::AudioCallbackHandler {

    std::vector<float> data;
    void audioDeviceIOCallback(const float *const *inputChannelData, int numInputChannels,
                               float *const *outputChannelData, int numOutputChannels,
                               int numSamples
    ) override {
        for (size_t i = 0; i < numSamples; i++) {
            data.emplace_back(inputChannelData[0][i]);
            outputChannelData[0][i] = 0;
        }
    }

};


// play predifined audio
struct PlayCallback : public ASIO::AudioCallbackHandler {

    size_t amp = 1;

    std::vector<float> data;
    void audioDeviceIOCallback(const float *const *inputChannelData, int numInputChannels,
                               float *const *outputChannelData, int numOutputChannels,
                               int numSamples
    ) override {
        size_t i = 0;
        for (; i < numSamples && tick + i < data.size(); i++) {
            outputChannelData[0][i] = amp * data[tick + i];
        }
        for (; i < numSamples; i++)
            outputChannelData[0][i] = 0;

        tick += numSamples;
    }

    void reset() {
        tick = 0;
    }

private:
    size_t tick = 0;

};

int main(int argc, char ** argv) {

    argparse::ArgumentParser program("test");
    program.add_argument("-a", "--amplifier").default_value("10.0");
    program.add_argument("-p", "--path").default_value("audio.bin");
    program.add_argument("-s", "--sample_rate").default_value("44100.0");
    program.add_argument("-t", "--duration").default_value("0");

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception &err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
    }

    auto amp = std::stof(program.get<std::string>("--amplifier"));
    auto sample_rate = std::stof(program.get<std::string>("--sample_rate"));
    auto path = program.get<std::string>("--path");
    auto duration = std::stoi(program.get<std::string>("--duration"));

    std::cout << "Amplifier: " << amp << std::endl;
    std::cout << "Path: " << path << std::endl;


    ASIO::AudioDevice asio;
    asio.open(1, 1, sample_rate);
    auto recordCallback = std::make_shared<RecordCallback>();
    auto playCallback = std::make_shared<PlayCallback>();

    playCallback->data = read_binary<float>(path);

    std::function<void()> pause;
    if (duration == 0) {
        pause = []() {
            std::puts("press enter to continue...");
            std::cin.get();
        };
    } else {
        pause = [duration]() {
            std::this_thread::sleep_for(std::chrono::seconds(duration));
        };
    }

    asio.start(recordCallback);
    asio.start(playCallback);
    pause();
    asio.stop(recordCallback);
    asio.stop(playCallback);

    playCallback->reset();
    playCallback->data = std::move(recordCallback->data);
    playCallback->amp = amp;

    asio.start(playCallback);
    pause();
    asio.stop(playCallback);

    return 0;
}

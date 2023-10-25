// this version adds two independent callbacks to the device manager in check2 phase1

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

#define CHECK 2

const float PI = acos(-1);
// duration of record and playback
constexpr size_t DURATION = 10;
//? why this 44100 works better than 48000?
constexpr size_t SAMPLERATE = 44100;

// used to store the recorded data in the program
static std::vector<float> recordingData;


// record sound
class RecordCallback : public ASIO::AudioCallbackHandler {

private:
    std::ofstream recordingFile;

public:

    RecordCallback() {
        recordingFile.open("recording.txt");
        recordingData.reserve(DURATION * SAMPLERATE);
    }
    ~RecordCallback() {
        recordingFile.close();
    }

    void audioDeviceIOCallback(const float *const *inputChannelData, int numInputChannels,
                               float *const *outputChannelData, int numOutputChannels,
                               int numSamples
    ) override {
        for (size_t i = 0; i < numSamples; i++) {
            recordingFile << inputChannelData[0][i] << '\n';
            recordingData.emplace_back(inputChannelData[0][i]);
            outputChannelData[0][i] = 0;
        }
    }

};

// play recorded audio
class PlaybackCallback : public ASIO::AudioCallbackHandler {

private:
    const size_t amp = 30;
    size_t samplesPlayed = 0;

public:
    void audioDeviceIOCallback(const float *const *inputChannelData, int numInputChannels,
                               float *const *outputChannelData, int numOutputChannels,
                               int numSamples
    ) override {
        for (size_t i = 0; i < numSamples && samplesPlayed + i < recordingData.size(); i++) {
            outputChannelData[0][i] = amp * recordingData[samplesPlayed + i];
        }
        samplesPlayed += numSamples;
    }

};

// play predifined audio
class PlayCallback : public ASIO::AudioCallbackHandler {

private:
    const size_t amp = 1;
    size_t samplesPlayed = 0;
    std::ifstream soundFile;
    std::vector<float> soundData;

public:

    PlayCallback() {
        soundFile.open("sound.txt");
        float datum;
        soundData.reserve(DURATION * SAMPLERATE);
        while (soundFile >> datum) {
            soundData.emplace_back(datum);
        }
        soundFile.close();
    }

    void audioDeviceIOCallback(const float *const *inputChannelData, int numInputChannels,
                               float *const *outputChannelData, int numOutputChannels,
                               int numSamples
    ) override {
        for (size_t i = 0; i < numSamples && samplesPlayed + i < soundData.size(); i++) {
            outputChannelData[0][i] = amp * soundData[samplesPlayed + i];
        }
        samplesPlayed += numSamples;
    }

};

int main() {

    ASIO::AudioDevice asio;
    asio.open(2, 2, 44100);
    auto recordCallback = std::make_shared<RecordCallback>();
    auto playbackCallback = std::make_shared<PlaybackCallback>();
    auto playCallback = std::make_shared<PlayCallback>();

    //------------------ record (may aslo play) -------------------
    // add callback(s) to AudioDeviceManager
    asio.start(recordCallback);
    std::cout << "recording";
#if CHECK==2
    asio.start(playCallback);
    std::cout << " & playing sound";
#endif
    std::cout << "... ";

    // wait for DURATION seconds and remove the callback(s)
    std::this_thread::sleep_for(std::chrono::seconds(DURATION));
    std::cout << "done" << std::endl << std::endl;
    asio.stop(recordCallback);
#if CHECK==2
    asio.stop(playCallback);
#endif

    //--------------------------- playback ------------------------
    // add callback to AudioDeviceManager
    asio.start(playbackCallback);
    std::cout << "playing recorded audio... ";

    // wait for DURATION seconds and remove the callback
    std::this_thread::sleep_for(std::chrono::seconds(DURATION));
    std::cout << "done" << std::endl;
    asio.stop(playbackCallback);

    return 0;
}

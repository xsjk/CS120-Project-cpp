// this version adds only one callback to the device manager in check2 phase1

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
constexpr size_t SAMPLERATE = 44100;

// used to store the recorded data in the program
static std::vector<float> recordingData;


// record sound
class RecordCallback : public AudioCallbackHandler {

private:
    std::ofstream recordingFile;

public:

    RecordCallback() {
        recordingFile.open("recording.txt");
        recordingData.reserve(DURATION * SAMPLERATE);
    }
    ~RecordCallback() override {
        recordingFile.close();
    }

    void audioDeviceIOCallback(const float *const *inputChannelData, int numInputChannels,
                               float *const *outputChannelData, int numOutputChannels,
                               int numSamples
    ) override {
        for (size_t i = 0; i < numSamples; i++) {
            recordingFile << inputChannelData[0][i] << '\n';
            recordingData.emplace_back(inputChannelData[0][i]);
        }
    }

};

// play predifined audio while recording
class PlayAndRecordCallback : public AudioCallbackHandler {

private:
    const float amp = 0.8;
    size_t samplesPlayed = 0;
    std::ifstream soundFile;
    std::vector<float> soundData;
    std::ofstream recordingFile;

public:

    PlayAndRecordCallback() {
        // play prepareation
        soundFile.open("sound.txt");
        float datum;
        soundData.reserve(DURATION * SAMPLERATE);
        while (soundFile >> datum) {
            soundData.emplace_back(datum);
        }
        soundFile.close();
        // record preparation
        recordingFile.open("recording.txt");
        recordingData.reserve(DURATION * SAMPLERATE);
    }
    ~PlayAndRecordCallback() {
        recordingFile.close();
    }

    void audioDeviceIOCallback(const float *const *inputChannelData, int numInputChannels,
                               float *const *outputChannelData, int numOutputChannels,
                               int numSamples
    ) override {
        for (size_t i = 0; i < numSamples && samplesPlayed + i < soundData.size(); i++) {
            outputChannelData[0][i] = amp * soundData[samplesPlayed + i];
        }
        samplesPlayed += numSamples;
        for (size_t i = 0; i < numSamples; i++) {
            recordingData.emplace_back(inputChannelData[0][i]);
        }
    }

};

// play recorded audio
class PlaybackCallback : public AudioCallbackHandler {

private:
    const float amp = 25;
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

int main() {

    /* Initialize Player */
    ASIODevice asio;
    asio.open(2, 2, 44100);

    // create callbacks
#if CHECK==1
    auto recordCallback = std::make_shared<RecordCallback>();
#elif CHECK==2
    auto playAndRecordCallback = std::make_shared<PlayAndRecordCallback>();
#endif
    auto playbackCallback = std::make_shared<PlaybackCallback>();

    //------------------ record (may aslo play) -------------------
    // add callback(s) to AudioDeviceManager
#if CHECK==1
    asio.start(recordCallback);
    std::cout << "recording... ";
#elif CHECK==2
    asio.start(playAndRecordCallback);
    std::cout << "playing predifined audio & recording ... ";
#endif

    // wait for DURATION seconds and remove the callback(s)
    std::this_thread::sleep_for(std::chrono::seconds(DURATION));
    std::cout << "done" << std::endl << std::endl;
#if CHECK==1
    asio.stop(recordCallback);
#elif CHECK==2
    asio.stop(playAndRecordCallback);
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

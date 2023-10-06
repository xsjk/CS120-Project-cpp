// this version adds two independent callbacks to the device manager in check2 phase1

#include <JuceHeader.h>
#include <fstream>

#define CHECK 2

const float PI = acos(-1);
// duration of record and playback
constexpr size_t DURATION = 10;
//? why this 44100 works better than 48000?
constexpr size_t SAMPLERATE = 44100;

// used to store the recorded data in the program
static std::vector<float> recordingData;

using namespace juce;

// record sound
class RecordCallback : public AudioIODeviceCallback {

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

    void audioDeviceAboutToStart(AudioIODevice* device) override {}
    void audioDeviceStopped() override {}

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
        float* const* outputChannelData, int numOutputChannels,
        int numSamples, const AudioIODeviceCallbackContext& context
    ) override {
        std::cout << outputChannelData[0][100] << ' ';
        for (size_t i = 0; i < numSamples; i++) {
            recordingFile << inputChannelData[0][i] << '\n';
            recordingData.emplace_back(inputChannelData[0][i]);
            outputChannelData[0][i] = 0;
        }
    }

};

// play recorded audio
class PlaybackCallback : public AudioIODeviceCallback {

private:
    const size_t amp = 30;
    size_t samplesPlayed = 0;

public:

    void audioDeviceAboutToStart(AudioIODevice* device) override {}
    void audioDeviceStopped() override {}

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
        float* const* outputChannelData, int numOutputChannels,
        int numSamples, const AudioIODeviceCallbackContext& context
    ) override {
        for (size_t i = 0; i < numSamples && samplesPlayed + i < recordingData.size(); i++) {
            outputChannelData[0][i] = amp * recordingData[samplesPlayed + i];
        }
		samplesPlayed += numSamples;
    }

};

// play predifined audio
class PlayCallback : public AudioIODeviceCallback {

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

    void audioDeviceAboutToStart(AudioIODevice* device) override {}
    void audioDeviceStopped() override {}

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
        float* const* outputChannelData, int numOutputChannels,
        int numSamples, const AudioIODeviceCallbackContext& context
    ) override {
        for (size_t i = 0; i < numSamples && samplesPlayed + i < soundData.size(); i++) {
            outputChannelData[0][i] = amp * soundData[samplesPlayed + i];
        }
		samplesPlayed += numSamples;
    }

};

int main() {

    /* Initialize Player */
    AudioDeviceManager dev_manager;
    // only one channel for both input and output
    dev_manager.initialiseWithDefaultDevices(1, 1);
	auto dev_info = dev_manager.getAudioDeviceSetup();
    dev_info.sampleRate = SAMPLERATE;
    dev_manager.setAudioDeviceSetup(dev_info, false);

    // create callbacks
    auto recordCallback = std::make_unique<RecordCallback>();
    auto playbackCallback = std::make_unique<PlaybackCallback>();
    auto playCallback = std::make_unique<PlayCallback>();

    //------------------ record (may aslo play) -------------------
    // add callback(s) to AudioDeviceManager
	dev_manager.addAudioCallback(recordCallback.get());
    std::cout << "recording";
#if CHECK==2
	dev_manager.addAudioCallback(playCallback.get());
    std::cout << " & playing sound";
#endif
    std::cout << "... ";

    // wait for DURATION seconds and remove the callback(s)
    std::this_thread::sleep_for(std::chrono::seconds(DURATION));
    std::cout << "done" << std::endl << std::endl;
    dev_manager.removeAudioCallback(recordCallback.get());
#if CHECK==2
    dev_manager.removeAudioCallback(playCallback.get());
#endif

    //--------------------------- playback ------------------------
    // add callback to AudioDeviceManager
	dev_manager.addAudioCallback(playbackCallback.get());
    std::cout << "playing recorded audio... ";

    // wait for DURATION seconds and remove the callback
    std::this_thread::sleep_for(std::chrono::seconds(DURATION));
    std::cout << "done" << std::endl;
    dev_manager.removeAudioCallback(playbackCallback.get());

    return 0;
}

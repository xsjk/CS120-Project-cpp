/*
  ==============================================================================

    This file contains the basic startup code for a JUCE application.

  ==============================================================================
*/

#include <JuceHeader.h>
#include <numbers>
#include <fstream>

#define PI acosf(-1)
#define SAMPLE_RATE 44100

using namespace juce;

class Tester : public AudioIODeviceCallback {

    std::ofstream file;

public:
    Tester() {
        file.open("recorded.txt");
    }
    void audioDeviceAboutToStart(AudioIODevice*) override {}

    void audioDeviceStopped() override {}


    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData,
        int numInputChannels,
        float* const* outputChannelData,
        int numOutputChannels,
        int numSamples,
        const AudioIODeviceCallbackContext& context
    ) override {
        for (int i = 0; i < numSamples; i++) {
            file << inputChannelData[0][i] << std::endl;
            for (int channel = 0; channel < numOutputChannels; channel++)
                outputChannelData[channel][i] = inputChannelData[0][i];
        }
    }


};

//==============================================================================
int main(int argc, char* argv[])
{
    /* Initialize Player */
    AudioDeviceManager dev_manager;
    dev_manager.initialiseWithDefaultDevices(2,2);
    AudioDeviceManager::AudioDeviceSetup dev_info;
    dev_info = dev_manager.getAudioDeviceSetup();
    dev_info.sampleRate = SAMPLE_RATE;
    dev_manager.setAudioDeviceSetup(dev_info, false);

    /* Add callback to AudioDeviceManager */
    auto tester = std::make_unique<Tester>();
    dev_manager.addAudioCallback(tester.get());

    /* Terminate the process */
    std::cout << "Press any ENTER to stop.\n";
    getchar();
    dev_manager.removeAudioCallback(tester.get());

    return 0;
}


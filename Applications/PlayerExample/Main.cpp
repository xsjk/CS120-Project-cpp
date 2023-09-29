/*
  ==============================================================================

    This file contains the basic startup code for a JUCE application.

  ==============================================================================
*/

#include <JuceHeader.h>

#define PI acosf(-1)
#define SAMPLE_RATE 44100

using namespace juce;

class Tester : public AudioIODeviceCallback {

    int freq = 262; // Hz
    float amp = 0.7f;
    float sampleRate = SAMPLE_RATE;
    int channelNum = 1;
    float dPhasePerSample = 2 * PI * (freq / sampleRate);
    long long step = 0;

public:
    void audioDeviceAboutToStart(AudioIODevice* device) override {}

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
            step++;
            outputChannelData[0][i] = amp * (
                + sin(2 * PI * (freq / sampleRate) * step)
                + sin(2 * PI * (freq * 1.26 / sampleRate) * step)
                + sin(2 * PI * (freq * 1.5 / sampleRate) * step)
                + sin(2 * PI * (freq * 1.78 / sampleRate) * step)
            ) / 4;
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


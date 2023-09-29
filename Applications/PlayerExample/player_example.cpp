/*
  ==============================================================================

    This file contains the basic startup code for a JUCE application.

  ==============================================================================
*/

#include <JuceHeader.h>

#define PI acos(-1)
#define SAMPLE_RATE 44100

using namespace juce;

class Tester : public AudioIODeviceCallback {

    float phase = 0.0f;
    int freq = 440; // Hz
    float amp = 0.7f;
    float sampleRate = SAMPLE_RATE;
    int channelNum = 1;
    float dPhasePerSample = 2 * PI * (freq / sampleRate);

public:
    Tester() {}

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
        for (int channel = 0; channel < numOutputChannels; ++channel) {
            for (int i = 0; i < numSamples; i++) {
                outputChannelData[channel][i] = amp * sin(phase += dPhasePerSample);
            }
        }
    }


};

//==============================================================================
int main(int argc, char* argv[])
{
    /* Initialize Player */
    AudioDeviceManager dev_manager;
    dev_manager.initialiseWithDefaultDevices(1,1);
    AudioDeviceManager::AudioDeviceSetup dev_info;
    dev_info = dev_manager.getAudioDeviceSetup();
    dev_info.sampleRate = SAMPLE_RATE;
    dev_manager.setAudioDeviceSetup(dev_info, false);

    /* Add callback to AudioDeviceManager */
    std::unique_ptr<Tester> tester;
    if (tester.get() == nullptr)
    {
        tester.reset(new Tester());
        dev_manager.addAudioCallback(tester.get());
    }

    /* Terminate the process */
    std::cout << "Press any ENTER to stop.\n";
    getchar();
    dev_manager.removeAudioCallback(tester.get());

    return 0;
}


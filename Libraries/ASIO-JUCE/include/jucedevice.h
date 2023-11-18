#pragma once

#include "jucecallback.hpp"

namespace JUCE {

    class Device : public std::enable_shared_from_this<Device> {
        juce::AudioDeviceManager dev_manager;
        juce::AudioDeviceManager::AudioDeviceSetup dev_info;
        std::shared_ptr<juce::AudioIODeviceCallback> callback;

    public:

        void open(int input_channels = 2, int output_channels = 2, double sampleRate = 48000);
        void start(const std::shared_ptr<IOHandler<float>>& callback);
        void stop();
        void close();

    };

}




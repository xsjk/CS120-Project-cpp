#include "jucedevice.h"

namespace JUCE {

void Device::open(int input_channels, int output_channels, double sampleRate) {
    dev_manager.initialiseWithDefaultDevices(2,2);
    dev_info =  dev_manager.getAudioDeviceSetup();
    dev_info.sampleRate = sampleRate;
    dev_manager.setAudioDeviceSetup(dev_info, false);
}

void Device::start(const std::shared_ptr<IOHandler<float>>& callback) {
    this->callback = std::make_shared<IOCallback>(callback, dev_info.sampleRate);
    dev_manager.addAudioCallback(this->callback.get());
}

void Device::stop() {

}

void Device::close() {

}


}
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include "asiosys.h"
#include "asiodrvr.h"
#include "asio.h"
#include "asiodrivers.h"


class ASIODevice;
class AudioCallbackHandler {
public:
    virtual void audioDeviceIOCallback(const float *const *inputChannelData, int numInputChannels,
                                       float **outputChannelData, int numOutputChannels,
                                       int numSamples) = 0;
    virtual void audioDeviceAboutToStart(ASIODevice *);
    virtual void audioDeviceStopped();
    virtual ~AudioCallbackHandler();
};

class ASIODevice {

    static ASIODevice *instance;
    static ASIOCallbacks callbacks;

    std::mutex callbackLock;

    AsioDrivers drivers;
    ASIODriverInfo driverInfo;
    std::shared_ptr<AudioCallbackHandler> callbackHandler;

    std::vector<ASIOBufferInfo> bufferInfo;
    std::vector<float> buffer;
    std::vector<float *> inBuffers, outBuffers;

    long numInputChans = 0, numOutputChans = 0;
    long bufferSize = 0;
    double currentSampleRate = 0;

public:
    ASIODevice(std::string name = "ASIO4ALL v2");
    ~ASIODevice();
    void open(int input_channels = 2, int output_channels = 2, ASIOSampleRate sample_rate = 44100);
    void start(std::shared_ptr<AudioCallbackHandler> newCallbackHandler);
    void stop();
    void close();
    void restart();

private:
    void callback(long index);
    static void convertFromFloat(const float *src, int *dest, int n) noexcept;
    static void convertToFloat(const int *src, float *dest, int n) noexcept;

};

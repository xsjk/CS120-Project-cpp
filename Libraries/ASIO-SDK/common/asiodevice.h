#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <memory>
#include "asio.h"
#include "asiodrivers.h"


class ASIODevice;
class AudioCallbackHandler {
public:
    virtual void audioDeviceIOCallback(const float *const *inputChannelData, int numInputChannels,
                                       float *const *outputChannelData, int numOutputChannels,
                                       int numSamples) = 0;
    virtual void audioDeviceAboutToStart(ASIODevice *);
    virtual void audioDeviceStopped();
    virtual ~AudioCallbackHandler();
};

class ASIODevice {

    static ASIODevice *instance;
    static ASIOCallbacks callbacks;


    AsioDrivers drivers;
    ASIODriverInfo driverInfo;
    std::vector<ASIOBufferInfo> bufferInfo;
    std::vector<int *> rawInBuffers, rawOutBuffers;

protected:
    std::mutex callbackLock;
    long numInputChans = 0, numOutputChans = 0;
    long bufferSize = 0;
    double currentSampleRate = 0;

public:
    ASIODevice(std::string name = "ASIO4ALL v2");
    ~ASIODevice();
    void open(int input_channels = 2, int output_channels = 2, ASIOSampleRate sample_rate = 44100);
    void close();
    virtual void restart();

protected:
    virtual void audioDeviceIOCallback(const int *const *inputChannelData, int *const *outputChannelData) = 0;

private:
    void callback(long index);

};


class ASIOAudioDevice : public ASIODevice {

    std::vector<float> buffer;
    std::vector<float *> inBuffers, outBuffers, tmpBuffers;
    std::unordered_set<std::shared_ptr<AudioCallbackHandler>> callbackHandlers;

public:
    using ASIODevice::ASIODevice;
    ~ASIOAudioDevice();
    void open(int input_channels = 2, int output_channels = 2, ASIOSampleRate sample_rate = 44100);
    void start(const std::shared_ptr<AudioCallbackHandler> &);
    void stop(const std::shared_ptr<AudioCallbackHandler> &);
    void restart() override;

protected:
    void audioDeviceIOCallback(const int *const *inputChannelData, int *const *outputChannelData) override;

private:
    static void convertFromFloat(const float *src, int *dest, int n) noexcept;
    static void convertToFloat(const int *src, float *dest, int n) noexcept;

};

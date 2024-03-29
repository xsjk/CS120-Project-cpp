#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <memory>
#include "asiosys.h"
#include "asiodrvr.h"
#include "asio.h"
#include "asiodrivers.h"
#include "asiocallback.hpp"

namespace ASIO {

    class Device;

    class ASIO_API AudioCallbackHandler {
    public:
        virtual void audioDeviceIOCallback(const float *const *inputChannelData, int numInputChannels,
                                        float *const *outputChannelData, int numOutputChannels,
                                        int numSamples) = 0;
        virtual void audioDeviceAboutToStart(Device *);
        virtual void audioDeviceStopped();
        virtual ~AudioCallbackHandler();
    };

    class ASIO_API Device {

        static Device *instance;
        static ASIOCallbacks callbacks;


        AsioDrivers drivers;
        ASIODriverInfo driverInfo;
        std::vector<ASIOBufferInfo> bufferInfo;
        std::shared_ptr<IOHandler<float>> ioHandler;
        std::vector<int *> rawInBuffers, rawOutBuffers;

    protected:
        std::mutex callbackLock;
        std::mutex ioHandlerLock;
        long numInputChans = 0, numOutputChans = 0;
        long bufferSize = 0;
        double currentSampleRate = 0;

    public:
        Device(std::string name = "ASIO4ALL v2");
        void open(int input_channels = 2, int output_channels = 2, double sample_rate = 44100);
        void start(std::shared_ptr<IOHandler<float>>);
        void stop();
        void close();
        virtual void restart();

    protected:
        virtual void audioDeviceIOCallback(const int *const *inputChannelData, int *const *outputChannelData);
    
    private:
        void callback(long index);

    };


    class ASIO_API AudioDevice : public Device {

        std::vector<float> buffer;
        std::vector<float *> inBuffers, outBuffers, tmpBuffers;
        std::unordered_set<std::shared_ptr<AudioCallbackHandler>> callbackHandlers;

    public:
        using Device::Device;
        ~AudioDevice();
        void open(int input_channels = 2, int output_channels = 2, double sample_rate = 44100);
        void start(const std::shared_ptr<AudioCallbackHandler> &);
        void stop(const std::shared_ptr<AudioCallbackHandler> &);
        void restart() override;

    protected:
        void audioDeviceIOCallback(const int *const *inputChannelData, int *const *outputChannelData) override;

    private:
        static void convertFromFloat(const float *src, int *dest, int n) noexcept;
        static void convertToFloat(const int *src, float *dest, int n) noexcept;

    };

}

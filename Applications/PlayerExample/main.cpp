// Audio Stream Input/Output library
#include <iostream>
#include <cmath>
#include <string>
#include <numbers>
#include <mutex>
#include <vector>
#include "asiosys.h"
#include "asiodrvr.h"
#include "asio.h"
#include "Windows.h"
#include "asiodrivers.h"

#define CATCH_ERROR(API) \
    do { \
        ASIOError result = API; \
        if (result != ASE_OK) { \
            std::cerr << #API << " failed with error code " << result << std::endl; \
            throw std::runtime_error(#API " failed at " __FILE__ ":" + std::to_string(__LINE__)); \
        } \
    } while (0)


class ASIODevice;
class AudioCallbackHandler {
public:
    virtual void audioDeviceIOCallback(const float *const *inputChannelData, int numInputChannels,
                                       float **outputChannelData, int numOutputChannels,
                                       int numSamples) = 0;
    virtual void audioDeviceAboutToStart(ASIODevice *) { }
    virtual void audioDeviceStopped() { }
    virtual ~AudioCallbackHandler() = default;
};

class ASIODevice {

    static ASIODevice *instance;

    std::mutex callbackLock;

    AsioDrivers drivers;
    ASIODriverInfo driverInfo;
    ASIOCallbacks callbacks {
        .bufferSwitch = [](long index, ASIOBool) { instance->callback(index); },
        .sampleRateDidChange = [](ASIOSampleRate) { },
        .asioMessage = [](long selector, long value, void *, double *) -> long {
            std::cout << "ASIO message: " << selector << ", " << value << std::endl;
            switch (selector) {
                case kAsioSelectorSupported:
                    if (value == kAsioResetRequest
                     || value == kAsioEngineVersion
                     || value == kAsioResyncRequest
                     || value == kAsioLatenciesChanged
                     || value == kAsioSupportsInputMonitor
                     || value == kAsioOverload)
                        return 1;
                    break;
                case kAsioBufferSizeChange: std::cout << "kAsioBufferSizeChange"; instance->restart(); return 1;
                case kAsioResetRequest:     std::cout << "kAsioResetRequest";     instance->restart(); return 1;
                case kAsioResyncRequest:    std::cout << "kAsioResyncRequest";    instance->restart(); return 1;
                case kAsioLatenciesChanged: std::cout << "kAsioLatenciesChanged"; return 1;
                case kAsioEngineVersion:    return 2;
                case kAsioSupportsTimeInfo:
                case kAsioSupportsTimeCode: return 0;
                case kAsioOverload:         std::cout << "kAsioOverload";         return 0;
            }
            return 0;
        },
        .bufferSwitchTimeInfo = [](ASIOTime *, long index, ASIOBool) -> ASIOTime * { instance->callback(index); return nullptr; },
    };
    std::shared_ptr<AudioCallbackHandler> callbackHandler;

    std::vector<ASIOBufferInfo> bufferInfo;
    std::vector<float> buffer;
    std::vector<float *> inBuffers, outBuffers;

    long numInputChans = 0, numOutputChans = 0;
    long bufferSize = 0;
    double currentSampleRate = 0;

public:
    ASIODevice(std::string name = "ASIO4ALL v2") {
        if (instance != nullptr)
            throw std::runtime_error("Only one ASIODevice can be created at a time");
        else
            instance = this;

        drivers.loadDriver(name.c_str());
        CATCH_ERROR(ASIOInit(&driverInfo));
    }

    ~ASIODevice() {
        instance = nullptr;
    }

    void open(
        int input_channels = 2,
        int output_channels = 2,
        ASIOSampleRate sample_rate = 44100
    ) {

        long maxInputChannels, maxOutputChannels;
        CATCH_ERROR(ASIOGetChannels(&maxInputChannels, &maxOutputChannels));
        if (input_channels > maxInputChannels || output_channels > maxOutputChannels)
            throw std::runtime_error("Requested number of channels exceeds maximum");

        numInputChans = input_channels;
        numOutputChans = output_channels;

        long minBufferSize, maxBufferSize, granularity;
        CATCH_ERROR(ASIOGetBufferSize(&minBufferSize, &maxBufferSize, &bufferSize, &granularity));
        std::cout << "Buffer size: " << bufferSize << " samples" << std::endl;

        bufferInfo.clear();
        inBuffers.clear();
        outBuffers.clear();
        auto totalChans = numInputChans + numOutputChans;
        buffer.resize(totalChans * bufferSize);
        for (int i = 0; i < numInputChans; ++i) {
            bufferInfo.push_back({ .isInput = true, .channelNum = i });
            inBuffers.push_back(&buffer[i * bufferSize]);
        }
        for (int i = 0; i < numOutputChans; ++i) {
            bufferInfo.push_back({ .isInput = false, .channelNum = i });
            outBuffers.push_back(&buffer[(i + numInputChans) * bufferSize]);
        }

        CATCH_ERROR(ASIOCanSampleRate(sample_rate));
        CATCH_ERROR(ASIOSetSampleRate(sample_rate));
        CATCH_ERROR(ASIOGetSampleRate(&currentSampleRate));
        std::cout << "Current sample rate: " << currentSampleRate << " Hz" << std::endl;

        CATCH_ERROR(ASIOCreateBuffers(bufferInfo.data(), bufferInfo.size(), bufferSize, &callbacks));
        std::cout << "Created " << bufferInfo.size() << " buffers" << std::endl;
        CATCH_ERROR(ASIOStart());
    }

    void start(std::shared_ptr<AudioCallbackHandler> newCallbackHandler) {
        if (newCallbackHandler) {
            newCallbackHandler->audioDeviceAboutToStart(this);
            std::lock_guard<std::mutex> lock(callbackLock);
            callbackHandler = newCallbackHandler;
        }
    }

    void stop() {
        std::lock_guard<std::mutex> lock(callbackLock);
        if (callbackHandler != nullptr)
            callbackHandler->audioDeviceStopped();
        callbackHandler = nullptr;
    }

    void close() {
        CATCH_ERROR(ASIOStop());
        CATCH_ERROR(ASIODisposeBuffers());
    }

    void restart() {
        std::cout << "Restart" << std::endl;
        auto oldCallback = callbackHandler;
        stop();
        close();
        open(numInputChans, numOutputChans, currentSampleRate);
        start(oldCallback);
    }

private:
    void callback(long bufferIndex) {

        std::lock_guard<std::mutex> lock(callbackLock);

        {
            auto samps = bufferSize;
            if (callbackHandler != nullptr) {
                for (int i = 0; i < inBuffers.size(); ++i)
                    convertToFloat((int *)bufferInfo[i].buffers[bufferIndex], inBuffers[i], bufferSize);

                callbackHandler->audioDeviceIOCallback(inBuffers.data(),
                                                       numInputChans,
                                                       outBuffers.data(),
                                                       numOutputChans,
                                                       samps);

                for (int i = 0; i < outBuffers.size(); ++i)
                    convertFromFloat(outBuffers[i], (int *)bufferInfo[inBuffers.size() + i].buffers[bufferIndex], bufferSize);

            }
            else {
                for (int i = 0; i < numOutputChans; ++i)
                    std::memset(bufferInfo[numInputChans + i].buffers[bufferIndex], 0, samps * sizeof(int));
            }
        }
        ASIOOutputReady();
    }


    static void convertFromFloat(const float *src, int *dest, int n) noexcept {
        constexpr float maxVal = 0x7fffffff;
        while (--n >= 0) {
            auto val = maxVal * *src++;
            *dest++ = round(val < -maxVal ? -maxVal : (val > maxVal ? maxVal : val));
        }
    }
    static void convertToFloat(const int *src, float *dest, int n) noexcept {
        constexpr float g = 1. / 0x7fffffff;
        while (--n >= 0) *dest++ = g * *src++;
    }

};

ASIODevice *ASIODevice::instance = nullptr;



class SineWave : public AudioCallbackHandler {

public:
    void audioDeviceIOCallback(const float *const *inputChannelData, int numInputChannels,
                               float **outputChannelData, int numOutputChannels,
                               int numSamples) override {
        constexpr float dphase = 2. * std::numbers::pi * 440. / 44100;
        static float phase = 0;
        for (int j = 0; j < numSamples; ++j) {
            auto y = sinf(phase += dphase);
            for (int i = 0; i < numOutputChannels && i < numInputChannels; ++i)
                outputChannelData[i][j] = y;
        }
    }

};



int main() {

    ASIODevice asio;
    asio.start(std::make_shared<SineWave>());
    asio.open(2, 2);
    std::getchar();

}
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
#include "asiodevice.h"

#define CATCH_ERROR(API) \
    do { \
        ASIOError result = API; \
        if (result != ASE_OK) { \
            std::cerr << #API << " failed with error code " << result << std::endl; \
            throw std::runtime_error(#API " failed at " __FILE__ ":" + std::to_string(__LINE__)); \
        } \
    } while (0)


AudioCallbackHandler::~AudioCallbackHandler() = default;
void AudioCallbackHandler::audioDeviceAboutToStart(ASIODevice *) { }
void AudioCallbackHandler::audioDeviceStopped() { }


ASIODevice::ASIODevice(std::string name) {
    if (instance != nullptr)
        throw std::runtime_error("Only one ASIODevice can be created at a time");
    else
        instance = this;
    drivers.loadDriver(name.c_str());
    CATCH_ERROR(ASIOInit(&driverInfo));
}

ASIODevice::~ASIODevice() {
    instance = nullptr;
    stop();
    close();
}

void ASIODevice::open(
    int input_channels,
    int output_channels,
    ASIOSampleRate sample_rate
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

void ASIODevice::start(std::shared_ptr<AudioCallbackHandler> newCallbackHandler) {
    if (newCallbackHandler) {
        newCallbackHandler->audioDeviceAboutToStart(this);
        std::lock_guard<std::mutex> lock(callbackLock);
        callbackHandler = newCallbackHandler;
    }
}

void ASIODevice::stop() {
    std::lock_guard<std::mutex> lock(callbackLock);
    if (callbackHandler != nullptr)
        callbackHandler->audioDeviceStopped();
    callbackHandler = nullptr;
}

void ASIODevice::close() {
    CATCH_ERROR(ASIOStop());
    CATCH_ERROR(ASIODisposeBuffers());
}

void ASIODevice::restart() {
    std::cout << "Restart" << std::endl;
    auto oldCallbackHandler = callbackHandler;
    stop();
    close();
    open(numInputChans, numOutputChans, currentSampleRate);
    start(oldCallbackHandler);
}

void ASIODevice::callback(long bufferIndex) {

    std::lock_guard<std::mutex> lock(callbackLock);
    {
        auto samps = bufferSize;
        if (callbackHandler) {
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

void ASIODevice::convertFromFloat(const float *src, int *dest, int n) noexcept {
    constexpr float maxVal = 0x7fffffff;
    while (--n >= 0) {
        auto val = maxVal * *src++;
        *dest++ = round(val < -maxVal ? -maxVal : (val > maxVal ? maxVal : val));
    }
}
void ASIODevice::convertToFloat(const int *src, float *dest, int n) noexcept {
    constexpr float g = 1. / 0x7fffffff;
    while (--n >= 0) *dest++ = g * *src++;
}

ASIODevice *ASIODevice::instance = nullptr;
ASIOCallbacks ASIODevice::callbacks {
    .bufferSwitch = [](long index, ASIOBool) { instance->callback(index); },
    .sampleRateDidChange = [](ASIOSampleRate) { },
    .asioMessage = [](long selector, long value, void *, double *) -> long {
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

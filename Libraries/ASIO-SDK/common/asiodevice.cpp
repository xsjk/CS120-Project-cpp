#include <iostream>
#include <cmath>
#include <cstring>
#include "asiosys.h"
#include "asiodrvr.h"
#include "asio.h"
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
}

ASIOAudioDevice::~ASIOAudioDevice() {
    auto oldCallbackHandler = callbackHandlers;
    for (auto &handler : oldCallbackHandler)
        stop(handler);
}

void ASIODevice::open(
    int input_channels,
    int output_channels,
    double sample_rate
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
    rawInBuffers.clear();
    rawOutBuffers.clear();
    rawInBuffers.resize(numInputChans);
    rawOutBuffers.resize(numOutputChans);
    for (int i = 0; i < numInputChans; ++i)
        bufferInfo.push_back({ .isInput = true, .channelNum = i });
    for (int i = 0; i < numOutputChans; ++i)
        bufferInfo.push_back({ .isInput = false, .channelNum = i });

    CATCH_ERROR(ASIOCanSampleRate(sample_rate));
    CATCH_ERROR(ASIOSetSampleRate(sample_rate));
    CATCH_ERROR(ASIOGetSampleRate(&currentSampleRate));
    std::cout << "Current sample rate: " << currentSampleRate << " Hz" << std::endl;

    CATCH_ERROR(ASIOCreateBuffers(bufferInfo.data(), bufferInfo.size(), bufferSize, &callbacks));
    std::cout << "Created " << bufferInfo.size() << " buffers" << std::endl;
    CATCH_ERROR(ASIOStart());
}


void ASIOAudioDevice::open(
    int input_channels,
    int output_channels,
    double sample_rate
) {
    ASIODevice::open(input_channels, output_channels, sample_rate);
    inBuffers.clear();
    outBuffers.clear();
    tmpBuffers.clear();
    buffer.resize((numInputChans + numOutputChans * 2) * bufferSize);
    
    for (int i = 0; i < numInputChans; ++i) {
        inBuffers.push_back(&buffer[i * bufferSize]);
    }
    for (int i = 0; i < numOutputChans; ++i) {
        outBuffers.push_back(&buffer[(i + numInputChans) * bufferSize]);
        tmpBuffers.push_back(&buffer[(i + numInputChans + numOutputChans) * bufferSize]);
    }
}


void ASIOAudioDevice::start(const std::shared_ptr<AudioCallbackHandler> &handler) {
    std::lock_guard<std::mutex> lock(callbackLock);
    callbackHandlers.emplace(handler);
    if (callbackHandlers.contains(handler)) {
        handler->audioDeviceAboutToStart(this);
    }
}

void ASIOAudioDevice::stop(const std::shared_ptr<AudioCallbackHandler> &handler) {
    std::lock_guard<std::mutex> lock(callbackLock);
    if (callbackHandlers.contains(handler)) {
        handler->audioDeviceStopped();
    }
    callbackHandlers.erase(handler);
}

void ASIODevice::close() {
    CATCH_ERROR(ASIOStop());
    CATCH_ERROR(ASIODisposeBuffers());
}

void ASIODevice::restart() {}

void ASIOAudioDevice::restart() {
    std::cout << "Restart" << std::endl;
    auto oldCallbackHandler = callbackHandlers;
    for (auto &handler : oldCallbackHandler)
        stop(handler);
    close();
    open(numInputChans, numOutputChans, currentSampleRate);
    for (auto &handler : oldCallbackHandler)
        start(handler);
}

void ASIODevice::callback(long bufferIndex) {

    std::lock_guard<std::mutex> lock(callbackLock);

    for (int i = 0; i < numInputChans; ++i)
        rawInBuffers[i] = (int *)bufferInfo[i].buffers[bufferIndex];
    for (int i = 0; i < numOutputChans; ++i)
        rawOutBuffers[i] = (int *)bufferInfo[numInputChans + i].buffers[bufferIndex];

    audioDeviceIOCallback(rawInBuffers.data(), rawOutBuffers.data());

}


void ASIOAudioDevice::audioDeviceIOCallback(const int *const *inputChannelData, int *const *outputChannelData) {

    for (int i = 0; i < numInputChans; ++i)
        convertToFloat(inputChannelData[i], inBuffers[i], bufferSize);

    auto it = callbackHandlers.begin();
    if (it != callbackHandlers.end()) {
        (*it)->audioDeviceIOCallback(inBuffers.data(), numInputChans, outBuffers.data(), numOutputChans, bufferSize);
        for (++it; it != callbackHandlers.end(); ++it) {
            (*it)->audioDeviceIOCallback(inBuffers.data(), numInputChans, tmpBuffers.data(), numOutputChans, bufferSize);
            for (int i = 0; i < numOutputChans; ++i)
                for (int j = 0; j < bufferSize; ++j)
                    outBuffers[i][j] += tmpBuffers[i][j];
        }
    }
    else {
        for (int i = 0; i < numOutputChans; ++i)
            std::memset(outBuffers[i], 0, bufferSize * sizeof(float));
    }
    for (int i = 0; i < numOutputChans; ++i)
        convertFromFloat(outBuffers[i], outputChannelData[i], bufferSize);

}

void ASIOAudioDevice::convertFromFloat(const float *src, int *dest, int n) noexcept {
    constexpr float maxVal = 0x7fffffff;
    while (--n >= 0) {
        auto val = maxVal * *src++;
        *dest++ = std::round(val < -maxVal ? -maxVal : (val > maxVal ? maxVal : val));
    }
}
void ASIOAudioDevice::convertToFloat(const int *src, float *dest, int n) noexcept {
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

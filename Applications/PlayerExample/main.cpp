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
        .sampleRateDidChange = nullptr,
        .asioMessage = nullptr,
        .bufferSwitchTimeInfo = nullptr,
    };
    std::shared_ptr<AudioCallbackHandler> currentCallback;

    std::vector<ASIOBufferInfo> buffer_info;
    std::vector<float> buffer;
    std::vector<float *> inBuffers, outBuffers;

    long numInputChans = 0, numOutputChans = 0;
    long minBufferSize = 0, maxBufferSize = 0, currentBlockSizeSamples = 0, bufferGranularity = 0;
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
        double sample_rate = 44100,
        int buffer_size = 512
    ) {

        numInputChans = input_channels;
        numOutputChans = output_channels;

        CATCH_ERROR(ASIOGetBufferSize(&minBufferSize, &maxBufferSize, &currentBlockSizeSamples, &bufferGranularity));
        buffer_info.clear();
        inBuffers.clear();
        outBuffers.clear();
        auto totalChans = numInputChans + numOutputChans;
        buffer.resize(totalChans * currentBlockSizeSamples);
        for (int i = 0; i < numInputChans; ++i) {
            buffer_info.push_back({ .isInput = true, .channelNum = i });
            inBuffers.push_back(&buffer[i * currentBlockSizeSamples]);
        }
        for (int i = 0; i < numOutputChans; ++i) {
            buffer_info.push_back({ .isInput = false, .channelNum = i });
            outBuffers.push_back(&buffer[(i + numInputChans) * currentBlockSizeSamples]);
        }

        CATCH_ERROR(ASIOCanSampleRate(sample_rate));
        CATCH_ERROR(ASIOSetSampleRate(sample_rate));
        CATCH_ERROR(ASIOGetSampleRate(&currentSampleRate));
        std::cout << "Current sample rate: " << currentSampleRate << " Hz" << std::endl;

        CATCH_ERROR(ASIOCreateBuffers(buffer_info.data(), buffer_info.size(), 512, &callbacks));
        std::cout << "Created " << buffer_info.size() << " buffers" << std::endl;
        CATCH_ERROR(ASIOStart());
        std::cout << "Started" << std::endl;
    }

    void start(std::shared_ptr<AudioCallbackHandler> callback) {
        if (callback) {
            callback->audioDeviceAboutToStart(this);
            std::lock_guard<std::mutex> lock(callbackLock);
            currentCallback = callback;
        }
    }

    void stop() {
        std::lock_guard<std::mutex> lock(callbackLock);
        if (currentCallback != nullptr)
            currentCallback->audioDeviceStopped();
        currentCallback = nullptr;
    }

    void close() {
        CATCH_ERROR(ASIOStop());
        CATCH_ERROR(ASIODisposeBuffers());
    }

private:
    void callback(long bufferIndex) {

        std::lock_guard<std::mutex> lock(callbackLock);

        {
            auto samps = currentBlockSizeSamples;
            if (currentCallback != nullptr) {
                for (int i = 0; i < inBuffers.size(); ++i)
                    convertToFloat((int *)buffer_info[i].buffers[bufferIndex], inBuffers[i], 512);

                currentCallback->audioDeviceIOCallback(inBuffers.data(),
                                                       numInputChans,
                                                       outBuffers.data(),
                                                       numOutputChans,
                                                       samps);

                for (int i = 0; i < outBuffers.size(); ++i)
                    convertFromFloat(outBuffers[i], (int *)buffer_info[inBuffers.size() + i].buffers[bufferIndex], 512);

            }
            else {
                for (int i = 0; i < numOutputChans; ++i)
                    std::memset(buffer_info[numInputChans + i].buffers[bufferIndex], 0, samps * sizeof(int));
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
                outputChannelData[i][j] = inputChannelData[i][j];
        }
    }

};



int main() {

    ASIODevice asio;
    asio.start(std::make_shared<SineWave>());
    asio.open(2, 2);
    std::getchar();

}
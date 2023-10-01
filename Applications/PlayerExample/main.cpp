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
    virtual void audioDeviceIOCallback (const float* const* inputChannelData, int numInputChannels,
                                        float** outputChannelData, int numOutputChannels,
                                        int numSamples) = 0;
    virtual void audioDeviceAboutToStart (ASIODevice*) {}
    virtual void audioDeviceStopped() {}
    virtual ~AudioCallbackHandler() = default;
};

class ASIODevice {

    static ASIODevice* instance;

    std::mutex callbackLock;

    AsioDrivers drivers;
    ASIODriverInfo driver_info;
    ASIOCallbacks callbacks {
        .bufferSwitch = [] (long index, ASIOBool) { instance->callback(index); },
        // .bufferSwitch = buffer_switch,
        .sampleRateDidChange = nullptr,
        .asioMessage = nullptr,
        .bufferSwitchTimeInfo = nullptr,
    };

    std::vector<ASIOBufferInfo> buffer_info;
    AudioCallbackHandler* currentCallback = nullptr;

    std::vector<float> buffer;
    std::vector<float*> inBuffers, outBuffers;

    long totalNumInputChans = 0, totalNumOutputChans = 0;
    long numActiveInputChans = 0, numActiveOutputChans = 0;
    long inputLatency = 0, outputLatency = 0;
    long minBufferSize = 0, maxBufferSize = 0, preferredBufferSize = 0, bufferGranularity = 0;
    int currentBlockSizeSamples = 0;
    double currentSampleRate = 0;

public:
    ASIODevice(std::string name="ASIO4ALL v2") {
        if (instance != nullptr)
            throw std::runtime_error("Only one ASIODevice can be created at a time");
        else
            instance = this;

        drivers.loadDriver(name.c_str());
        CATCH_ERROR(ASIOInit(&driver_info));
    }

    ~ASIODevice() {
        instance = nullptr;
    }

    void open(
        int input_channels=2,
        int output_channels=2,
        double sample_rate=44100,
        int buffer_size=512
    ) {
        
        CATCH_ERROR(ASIOGetChannels(&totalNumInputChans, &totalNumOutputChans));
        numActiveInputChans = input_channels;
        numActiveOutputChans = output_channels;
        if (numActiveInputChans > totalNumInputChans) 
            throw std::runtime_error("Requested number of input channels is greater than the number of available input channels");
        if (numActiveOutputChans > totalNumOutputChans)
            throw std::runtime_error("Requested number of output channels is greater than the number of available output channels");

        CATCH_ERROR(ASIOGetLatencies(&inputLatency, &outputLatency));
        CATCH_ERROR(ASIOGetBufferSize(&minBufferSize, &maxBufferSize, &preferredBufferSize, &bufferGranularity));

        currentBlockSizeSamples = preferredBufferSize;
        buffer_info.clear();
        inBuffers.clear();
        outBuffers.clear();
        auto totalChans = totalNumInputChans + totalNumOutputChans;
        buffer.resize(totalChans * currentBlockSizeSamples);
        for (int i = 0; i < totalNumInputChans; ++i) {
            buffer_info.push_back(ASIOBufferInfo{.isInput = true, .channelNum = i});
            inBuffers.push_back(&buffer[i * currentBlockSizeSamples]);
        }
        for (int i = 0; i < totalNumOutputChans; ++i) {
            buffer_info.push_back(ASIOBufferInfo{.isInput = false, .channelNum = i});
            outBuffers.push_back(&buffer[(i + totalNumInputChans) * currentBlockSizeSamples]);
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

    void start(AudioCallbackHandler* callback) {
        if (callback) {
            callback->audioDeviceAboutToStart(this);
            std::lock_guard<std::mutex> lock (callbackLock);
            currentCallback = callback;
        }
    }

    void stop() {
        std::lock_guard<std::mutex> lock (callbackLock);
        if (currentCallback != nullptr)
            currentCallback->audioDeviceStopped();
        currentCallback = nullptr;
    }

    void close() {
        CATCH_ERROR(ASIOStop());
        CATCH_ERROR(ASIODisposeBuffers());
    }


    
    void callback (long bufferIndex) {

        std::lock_guard<std::mutex> lock (callbackLock);

        {
            auto samps = currentBlockSizeSamples;
            if (currentCallback != nullptr) {

                for (int i = 0; i < inBuffers.size(); ++i)
                    convertToFloat ((int*)buffer_info[i].buffers[bufferIndex], inBuffers[i], 512);

                currentCallback->audioDeviceIOCallback (inBuffers.data(),
                                                        numActiveInputChans,
                                                        outBuffers.data(),
                                                        numActiveOutputChans,
                                                        samps);

                for (int i = 0; i < outBuffers.size(); ++i)
                    convertFromFloat (outBuffers[i], (int*)buffer_info[2 + i].buffers[bufferIndex], 512);

            } else {
                for (int i = 0; i < numActiveOutputChans; ++i)
                    std::memset(buffer_info[numActiveInputChans + i].buffers[bufferIndex], 0, samps * sizeof(int));
            }
        }
        ASIOOutputReady();
    }


    static void convertFromFloat (const float* src, int* dest, int n) noexcept {
        constexpr float maxVal = 0x7fffffff;
        while (--n >= 0) {
            auto val = maxVal * *src++;
            *dest++ = round(val < -maxVal ? -maxVal : (val > maxVal ? maxVal : val));
        }
    }
    static void convertToFloat (const int* src, float* dest, int n) noexcept {
        constexpr float g = 1. / 0x7fffffff;
        while (--n >= 0) *dest++ = g * *src++;
    }

};

ASIODevice *ASIODevice::instance = nullptr;



class SineWave : public AudioCallbackHandler {
    
public:

    void audioDeviceIOCallback (const float* const* inputChannelData, int numInputChannels,
                                float** outputChannelData, int numOutputChannels,
                                int numSamples) override {
        {
            constexpr float dphase = 2. * std::numbers::pi * 440. / 44100;
            static float phase = 0;
            for (int j = 0; j < numSamples; ++j) {
                auto y = sinf(phase += dphase);
                outputChannelData[0][j] = inputChannelData[0][j];
            }
        }
    }

};



int main() {
    ASIODevice asio;
    asio.open(1, 1);
    auto sinewave = std::make_unique<SineWave>();
    asio.start(sinewave.get());

    // Sleep(1000);
    std::getchar();
}
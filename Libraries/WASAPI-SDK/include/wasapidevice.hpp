#pragma once

#include "wasapi.hpp"
#include "wasapicallback.hpp"
#include <assert.h>

struct AudioClientHandler {

    static WASAPI::DeviceEnumerator pEnumerator;
    WASAPI::Device pDevice;
    WASAPI::AudioClient pAudioClient;
    WASAPI::WaveFormat pWaveFormat;
    WASAPI::Handle eventHandle;
    AUDCLNT_SHAREMODE shareMode;

    AudioClientHandler(AUDCLNT_SHAREMODE shareMode) : shareMode { shareMode } { }

    void set_wave_format(WORD channels, DWORD sampleRate, WORD bitWidth) {
        set_wave_format(WAVEFORMATEX {
            .wFormatTag = 1,
            .nChannels = channels,
            .nSamplesPerSec = sampleRate,
            .nAvgBytesPerSec = sampleRate * channels * bitWidth / CHAR_BIT,
            .nBlockAlign = WORD(channels * bitWidth / CHAR_BIT),
            .wBitsPerSample = bitWidth,
            .cbSize = 0
        });
    }

    void set_wave_format(WAVEFORMATEX format) {
        pWaveFormat = pAudioClient.get_mix_format();
        *pWaveFormat = format;
        if (!pAudioClient.is_format_supported(shareMode, pWaveFormat))
            throw std::runtime_error("Wave format not supported");
        std::cout << pWaveFormat << std::endl;
    }

    void initialize() {

        auto exclusiveMinDeviceDuration = pAudioClient.test_min_device_period(shareMode, pWaveFormat);
        
        if (!(eventHandle = WASAPI::Handle(SYNCHRONIZE)))
            throw std::runtime_error("Cannot create audio ready event");

        pAudioClient.set_event_handle(eventHandle);
    }

};


WASAPI::DeviceEnumerator AudioClientHandler::pEnumerator;

    

class WASAPIDevice {
private:
    std::shared_ptr<WASAPIIOHandler> callbackHandler;

    
    template <class C>
    struct Process : public AudioClientHandler {
        C client;

        Process(AUDCLNT_SHAREMODE shareMode) : AudioClientHandler(shareMode) {
            constexpr auto flow = [] {
                if constexpr(std::is_same_v<C, WASAPI::AudioCaptureClient>) return eCapture;
                else if constexpr(std::is_same_v<C, WASAPI::AudioRenderClient>) return eRender;
            }();
            pDevice = pEnumerator.get_default_device(flow);
            pAudioClient = pDevice.get_client();
        }

        void initialize() {
            AudioClientHandler::initialize();
            if constexpr(std::is_same_v<C, WASAPI::AudioRenderClient>) client = pAudioClient.get_render_client();
            else if constexpr(std::is_same_v<C, WASAPI::AudioCaptureClient>) client = pAudioClient.get_capture_client();
        }

    };
    
    Process<WASAPI::AudioCaptureClient> recorder;
    Process<WASAPI::AudioRenderClient> player;
    UINT32 playerBufferSize, recorderBufferSize;

    WASAPI::EventTrigger trigger;

public:
    WASAPIDevice(
        AUDCLNT_SHAREMODE captureMode = AUDCLNT_SHAREMODE_EXCLUSIVE,
        AUDCLNT_SHAREMODE renderMode = AUDCLNT_SHAREMODE_EXCLUSIVE
    ) : 
        recorder { captureMode }, player { renderMode } 
    {
           

    }

    void open(WORD channels=2, DWORD sampleRate=48000, WORD bitWidth=16) {
        recorder.set_wave_format(channels, sampleRate, bitWidth);
        player.set_wave_format(channels, sampleRate, bitWidth);
        recorder.initialize();
        player.initialize();
        playerBufferSize = player.pAudioClient.get_buffer_size();
        recorderBufferSize = recorder.pAudioClient.get_buffer_size();
        std::cout << "Player buffer size: " << playerBufferSize << std::endl;
        std::cout << "Recorder buffer size: " << recorderBufferSize << std::endl;
             
        trigger.add(recorder.eventHandle, [&] {
            auto [pData, numFrames, flags] = recorder.client.get_buffer();
            if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) {
                std::cerr << "AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY" << std::endl;
            }
            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                std::cerr << "AUDCLNT_BUFFERFLAGS_SILENT" << std::endl;
            }
            if (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR) {
                std::cerr << "AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR" << std::endl;
            }
            callbackHandler->inputCallback(pData, numFrames);
            recorder.client.release_buffer(numFrames);
        });

        trigger.add(player.eventHandle, [&] {
            auto pData = player.client.get_buffer(playerBufferSize);
            callbackHandler->outputCallback(playerBufferSize, pData);
            player.client.release_buffer(playerBufferSize);
        });

    }

    void start(std::shared_ptr<WASAPIIOHandler> callback) {
        callbackHandler = std::move(callback);
        recorder.pAudioClient.start();
        player.pAudioClient.start();
        trigger.start();
    }

    void stop() {
        trigger.stop();
        recorder.pAudioClient.stop();
        player.pAudioClient.stop();
        callbackHandler = nullptr;
    }

};


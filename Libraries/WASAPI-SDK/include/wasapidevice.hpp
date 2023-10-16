#pragma once

#include "wasapi.hpp"


class WASAPIIOHandler {
  public:
    virtual void inputCallback(BYTE* pBuffer, std::size_t availableFrameCnt) noexcept = 0;
    virtual void outputCallback(std::size_t availableFrameCnt, BYTE* pBuffer) noexcept = 0;
    virtual ~WASAPIIOHandler() {}
};


struct AudioClientHandler {

    static WASAPI::DeviceEnumerator pEnumerator;
    WASAPI::Device pDevice;
    WASAPI::AudioClient pAudioClient;
    WASAPI::WaveFormat pWaveFormat;
    WASAPI::Handle synchronizer;
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
        
        if (!(synchronizer = WASAPI::Handle(SYNCHRONIZE)))
            throw std::runtime_error("Cannot create audio ready event");

        pAudioClient.set_event_handle(synchronizer);
    }

};


WASAPI::DeviceEnumerator AudioClientHandler::pEnumerator;

namespace WASAPI {

}

struct RecordProcess : public AudioClientHandler {

    WASAPI::AudioCaptureClient pCaptureClient;

    RecordProcess(AUDCLNT_SHAREMODE shareMode) : AudioClientHandler(shareMode) {
        pDevice = pEnumerator.get_default_device(eCapture);
        pAudioClient = pDevice.get_client();
    }
    void initialize() {
        AudioClientHandler::initialize();
        pCaptureClient = pAudioClient.get_capture_client();
    }

};

struct RenderProcess : public AudioClientHandler {

    WASAPI::AudioRenderClient pRenderClient;

    RenderProcess(AUDCLNT_SHAREMODE shareMode) : AudioClientHandler(shareMode) {
        pDevice = pEnumerator.get_default_device(eRender);
        pAudioClient = pDevice.get_client();
    }
    void initialize() {
        AudioClientHandler::initialize();
        pRenderClient = pAudioClient.get_render_client();
    }

};

class WASAPIDevice {
private:
    std::shared_ptr<WASAPIIOHandler> callbackHandler;
    RecordProcess recorder;
    RenderProcess player;

public:
    WASAPIDevice(
        AUDCLNT_SHAREMODE captureMode = AUDCLNT_SHAREMODE_EXCLUSIVE,
        AUDCLNT_SHAREMODE renderMode = AUDCLNT_SHAREMODE_EXCLUSIVE
    ) : 
        recorder { captureMode }, player { renderMode } { }

    void open(WORD channels=2, DWORD sampleRate=48000, WORD bitWidth=16) {
        recorder.set_wave_format(channels, sampleRate, bitWidth);
        player.set_wave_format(channels, sampleRate, bitWidth);
        recorder.initialize();
        player.initialize();
    }

    void start(std::shared_ptr<WASAPIIOHandler> callback) {
        callbackHandler = std::move(callback);
    }

    void stop(const std::shared_ptr<WASAPIIOHandler>& callback) {
        callbackHandler = nullptr;
    }

    void mainloop() {

        DWORD waitResults;
        UINT32 numFramesAvailable = 0;
        UINT32 renderMaxFrames = 0;
        bool bDone = 0;
        DWORD flags;
        HANDLE hTask = nullptr;
        DWORD taskIndex = 0;

        hTask = AvSetMmThreadCharacteristics("Pro Audio", &taskIndex);
        if (!hTask) {
            throw std::runtime_error("boost the thread priority!");
        }

        renderMaxFrames = player.pAudioClient.get_buffer_size();

        recorder.pAudioClient.start();
        player.pAudioClient.start();
        
        while (true) {
            HANDLE arr[2] = { recorder.synchronizer.get(), player.synchronizer.get() };
            waitResults = WaitForMultipleObjects(sizeof(arr) / sizeof(*arr), arr, FALSE, INFINITE);

            switch (waitResults) {
                case WAIT_OBJECT_0 + 0:
                    // record
                    {
                        auto [pData, numFramesAvailable, flags] = recorder.pCaptureClient.get_buffer();
                            
                            if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) {
                                
                            }
                            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                                pData = nullptr;
                            }
                            if (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR) {
                                std::cerr << "AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR" << std::endl;
                            }

                        callbackHandler->inputCallback(pData, numFramesAvailable);
                        recorder.pCaptureClient.release_buffer(numFramesAvailable);
                    }
                    break;

                case WAIT_OBJECT_0 + 1:
                    // play
                    {
                        auto pData = player.pRenderClient.get_buffer(renderMaxFrames);
                        callbackHandler->outputCallback(renderMaxFrames, pData);
                        player.pRenderClient.release_buffer(renderMaxFrames);
                    }
                    break;

                default:
                    std::cout << "WaitResults: " << waitResults << std::endl;

            }

        }

        recorder.pAudioClient.stop();
        player.pAudioClient.stop();

        if (hTask)
            AvRevertMmThreadCharacteristics(hTask);
    }

};


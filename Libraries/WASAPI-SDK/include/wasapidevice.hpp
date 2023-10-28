#pragma once

#include "wasapi.hpp"
#include "wasapicallback.hpp"


namespace WASAPI {

    struct ClientHandler {

        static WASAPI::DeviceEnumerator pEnumerator;
        WASAPI::MMDevice pDevice;
        WASAPI::AudioClient pAudioClient;
        WASAPI::WaveFormat pWaveFormat;
        WASAPI::Handle eventHandle;
        AUDCLNT_SHAREMODE shareMode;

        ClientHandler(AUDCLNT_SHAREMODE shareMode) : shareMode { shareMode } { }

        void set_wave_format(WORD channels, DWORD sampleRate, WORD bitWidth) {
            set_wave_format({
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
            // std::cout << pWaveFormat << std::endl;
        }

        void initialize() {

            auto exclusiveMinDeviceDuration = pAudioClient.test_min_device_period(shareMode, pWaveFormat);

            if (!(eventHandle = WASAPI::Handle(SYNCHRONIZE)))
                throw std::runtime_error("Cannot create audio ready event");

            pAudioClient.set_event_handle(eventHandle);
        }

    };


    DeviceEnumerator ClientHandler::pEnumerator;



    class Device {
    private:
        std::shared_ptr<WASAPI::IOHandler> callbackHandler;

        template <class C>
        struct Process : public ClientHandler {
            C client;

            Process(AUDCLNT_SHAREMODE shareMode) : ClientHandler(shareMode) {
                constexpr auto flow = [] {
                    if constexpr (std::is_same_v<C, WASAPI::AudioCaptureClient>) return eCapture;
                    else if constexpr (std::is_same_v<C, WASAPI::AudioRenderClient>) return eRender;
                }();
                pDevice = pEnumerator.get_default_device(flow);
                pAudioClient = pDevice.get_client();
            }

            void initialize() {
                ClientHandler::initialize();
                if constexpr (std::is_same_v<C, WASAPI::AudioRenderClient>) client = pAudioClient.get_render_client();
                else if constexpr (std::is_same_v<C, WASAPI::AudioCaptureClient>) client = pAudioClient.get_capture_client();
            }

        };

        Process<WASAPI::AudioCaptureClient> recorder;
        Process<WASAPI::AudioRenderClient> player;
        UINT32 playerBufferSize, recorderBufferSize;

        WASAPI::EventTrigger trigger;

    public:
        Device(
            AUDCLNT_SHAREMODE captureMode = AUDCLNT_SHAREMODE_EXCLUSIVE,
            AUDCLNT_SHAREMODE renderMode = AUDCLNT_SHAREMODE_EXCLUSIVE
        ) :
            recorder { captureMode }, player { renderMode } { }

        void open(WORD input_channels = 2, WORD output_channels = 2, DWORD sampleRate = 48000) {
            recorder.set_wave_format(input_channels, sampleRate, 16);
            player.set_wave_format(output_channels, sampleRate, 16);
            recorder.initialize();
            player.initialize();
            playerBufferSize = player.pAudioClient.get_buffer_size();
            recorderBufferSize = recorder.pAudioClient.get_buffer_size();
            std::cout << "Player buffer size: " << playerBufferSize << std::endl;
            std::cout << "Recorder buffer size: " << recorderBufferSize << std::endl;

            trigger.add(recorder.eventHandle, [&] {
                auto [pData, numFrames, flags] = recorder.client.get_buffer();
                if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
                    std::cerr << "AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY" << std::endl;
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                    std::cerr << "AUDCLNT_BUFFERFLAGS_SILENT" << std::endl;
                if (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)
                    std::cerr << "AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR" << std::endl;
                auto view = DataView(pData, numFrames, sampleRate);
                callbackHandler->inputCallback(view);
                recorder.client.release_buffer(numFrames);
            });

            trigger.add(player.eventHandle, [&] {
                auto pData = player.client.get_buffer(playerBufferSize);
                auto view = DataView(pData, playerBufferSize, sampleRate);
                callbackHandler->outputCallback(view);
                player.client.release_buffer(playerBufferSize);
            });

        }
        
        ~Device() {
            if (callbackHandler)
                stop();
        }

        void start(const std::shared_ptr<IOHandler>& callback) {
            callbackHandler = callback;
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

}




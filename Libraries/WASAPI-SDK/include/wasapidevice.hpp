#pragma once

#include "wasapi.hpp"
#include "wasapicallback.hpp"

#include <cassert>

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



    class Device : public std::enable_shared_from_this<Device> {
    private:
        std::shared_ptr<WASAPI::IOHandler<float>> callbackHandler;
        std::mutex mutex;

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
            std::shared_ptr<Device> self;
            try {
                self = shared_from_this();
            } catch (const std::bad_weak_ptr&) {
                std::cerr << "You must create Device with std::make_shared<Device>(...)" << std::endl;
            }

            recorder.set_wave_format(input_channels, sampleRate, 16);
            player.set_wave_format(output_channels, sampleRate, 16);
            recorder.initialize();
            player.initialize();
            playerBufferSize = player.pAudioClient.get_buffer_size();
            recorderBufferSize = recorder.pAudioClient.get_buffer_size();
            std::cout << "Player buffer size: " << playerBufferSize << std::endl;
            std::cout << "Recorder buffer size: " << recorderBufferSize << std::endl;

            trigger.add(recorder.eventHandle, [
                self=self, 
                sampleRate=sampleRate
            ] {
                std::lock_guard<std::mutex> lock(self->mutex);
                if (self->callbackHandler == nullptr) return;
                auto [pData, numFrames, flags] = self->recorder.client.get_buffer();
                if (pData == nullptr) return;
                if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
                    std::cerr << "AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY" << std::endl;
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                    std::cerr << "AUDCLNT_BUFFERFLAGS_SILENT" << std::endl;
                if (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)
                    std::cerr << "AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR" << std::endl;
                auto view = DataView<float>(pData, numFrames, sampleRate);
                self->callbackHandler->inputCallback(view);
                self->recorder.client.release_buffer(numFrames);
                
            });

            trigger.add(player.eventHandle, [
                self=self, 
                bufferSize=playerBufferSize, 
                sampleRate=sampleRate
            ] {
                std::lock_guard<std::mutex> lock(self->mutex);
                if (self->callbackHandler == nullptr) return;
                auto pData = self->player.client.get_buffer(bufferSize);
                auto view = DataView<float>(pData, bufferSize, sampleRate);
                self->callbackHandler->outputCallback(view);
                self->player.client.release_buffer(bufferSize);
            });


        }
        
        ~Device() {
            close();
        }

        void start(const std::shared_ptr<IOHandler<float>>& callback) {
            std::lock_guard<std::mutex> lock(mutex);
            callbackHandler = callback;
            recorder.pAudioClient.start();
            player.pAudioClient.start();
            trigger.start();
        }

        void stop() {
            std::lock_guard<std::mutex> lock(mutex);
            if (!callbackHandler) return;
            trigger.stop();
            recorder.pAudioClient.stop();
            player.pAudioClient.stop();
            callbackHandler = nullptr;
        }

        void close() {
            stop();
        }

    };

}




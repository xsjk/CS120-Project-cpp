#ifndef CORE_PROCESS_HPP
#define CORE_PROCESS_HPP

#include "constant.hpp"
#include "utils.hpp"
#include "audio_io_project_base.hpp"

#include <avrt.h>
#include <memory>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <iomanip>
#include <cstdint>

#define CATCH_ERROR(API) \
    do { \
        auto result = (API); \
        if (result != S_OK) { \
            std::cerr << #API << " failed with error code 0x" << std::hex << result << std::dec << " at " __FILE__ ":" << __LINE__ << std::endl; \
            throw std::runtime_error(#API " failed at " __FILE__ ":" + std::to_string(__LINE__)); \
        } \
    } while (0)



struct AudioClientHandler {

    static IMMDeviceEnumerator *pEnumerator;
    IMMDevice *pDevice = nullptr;
    IAudioClient *pAudioClient = nullptr;
    WAVEFORMATEX *pWaveFormat = nullptr;
    HANDLE waitArray = nullptr;
    REFERENCE_TIME processDuration = 0;
    AUDCLNT_SHAREMODE shareMode = AUDCLNT_SHAREMODE_EXCLUSIVE;
    union {
        IUnknown* client;
        IAudioRenderClient* pRenderClient;
        IAudioCaptureClient* pCaptureClient;
    };

    inline void activate() { 
        pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void **)(&pAudioClient)); 
        if (!pAudioClient)
            throw std::runtime_error("Cannot activate audio client");
    }

    inline void set_wave_format(WORD channels, DWORD sampleRate, WORD bitWidth) {
        set_wave_format(WAVEFORMATEX{
            .wFormatTag = 1,
            .nChannels = channels,
            .nSamplesPerSec = sampleRate,
            .nAvgBytesPerSec = sampleRate * channels * bitWidth / CHAR_BIT,
            .nBlockAlign = WORD(channels * bitWidth / CHAR_BIT),
            .wBitsPerSample = bitWidth,
            .cbSize = 0
        });
    }

    inline void set_wave_format(WAVEFORMATEX format) {
        CATCH_ERROR(pAudioClient->GetMixFormat(&pWaveFormat)); // malloc space for *pWaveFormat
        *pWaveFormat = format;

        WAVEFORMATEX *pClosest = nullptr;
        switch (pAudioClient->IsFormatSupported(shareMode, pWaveFormat, &pClosest)) {
            case S_OK:
                break;
            case S_FALSE:
                if (shareMode == AUDCLNT_SHAREMODE_EXCLUSIVE)
                    goto error;
                *pWaveFormat = *pClosest;
                CoTaskMemFree(pClosest);
                pClosest = nullptr;
                break;
            default:
                goto error;
        }
        pWaveFormat = pWaveFormat;
        std::cout << pWaveFormat << std::endl;
        return;

        error:
            if (pClosest) {
                CoTaskMemFree(pClosest);
                throw std::runtime_error("Wave format not supported");
            }
    }

    inline void initialize() {
        
        MyErrorCode error = S_OK;
        REFERENCE_TIME defaultDevicePeriod;
        REFERENCE_TIME minDevicePeriod;

        CATCH_ERROR(pAudioClient->GetDevicePeriod(&defaultDevicePeriod, &minDevicePeriod));
        std::cout << "Default Device Period is " << defaultDevicePeriod / referenceTimePerMillisec << "ms\n";
        std::cout << "Minimum Device Period is " << minDevicePeriod / referenceTimePerMillisec << "ms\n";

        CATCH_ERROR(pAudioClient->Initialize(shareMode, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, minDevicePeriod, (shareMode == AUDCLNT_SHAREMODE_SHARED ? 0 : minDevicePeriod), pWaveFormat, nullptr));
        processDuration = minDevicePeriod;

        REFERENCE_TIME streamLatency;
        UINT32 bufferFrameSize;

        error = pAudioClient->GetStreamLatency(&streamLatency);
        ERROR_HANDLER(error)
            std::cout << "The stream latency is " << streamLatency / referenceTimePerMillisec << "ms\n";

        error = pAudioClient->GetBufferSize(&bufferFrameSize);
        ERROR_HANDLER(error)
            std::cout << "The buffer has " << bufferFrameSize << "frames\n";
        std::cout << "The size of each frame is " << pWaveFormat->wBitsPerSample * pWaveFormat->nChannels / CHAR_BIT << "bytes\n";

        HANDLE hAudioReadyEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

        if (!hAudioReadyEvent) {
            throw std::runtime_error("Cannot create audio ready event");
        }

        CATCH_ERROR(pAudioClient->SetEventHandle(hAudioReadyEvent));

        CATCH_ERROR(pAudioClient->GetService(IID_IAudioCaptureClient, (void**)&client));

        waitArray = hAudioReadyEvent;

    }

};

IMMDeviceEnumerator *AudioClientHandler::pEnumerator = nullptr;

struct RecordProcess : public AudioClientHandler {

};

struct RenderProcess : public AudioClientHandler {

};

class CoreProcess {
private:
    std::shared_ptr<AudioIOProjectBase> _callbackHandler;
    IMMDeviceEnumerator *_pEnumerator = nullptr;
    IMMDevice *_pDevices[2] = { nullptr, nullptr };
    IAudioClient *_pAudioClients[2] = { nullptr, nullptr };
    WAVEFORMATEX *_pWaveFormats[2] = { nullptr, nullptr };
    IAudioCaptureClient *_pCaptureClient = nullptr;
    IAudioRenderClient *_pRenderClient = nullptr;
    HANDLE _waitArray[2] = { nullptr, nullptr };
    AUDCLNT_SHAREMODE _shareMode[2];
    REFERENCE_TIME _processDuration[2] = { 0, 0 };

    std::size_t channel = 2;
    std::size_t sampleRate = 48000;
    std::size_t bitWidth = 16;
    std::size_t frameBytes = channel * bitWidth / CHAR_BIT;

public:
    inline CoreProcess(std::shared_ptr<AudioIOProjectBase> projectBase,
                       AUDCLNT_SHAREMODE captureMode,
                       AUDCLNT_SHAREMODE renderMode) noexcept :
        _callbackHandler { std::move(projectBase) },
        _shareMode { captureMode, renderMode } {
        CoInitializeEx(nullptr, 0);
    }

    inline std::shared_ptr<AudioIOProjectBase> get_project_base() noexcept { return _callbackHandler; }

    inline void core_process() {
        CATCH_ERROR(CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void **)(&_pEnumerator)));
        CATCH_ERROR(_pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &_pDevices[0])); // record device
        CATCH_ERROR(_pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &_pDevices[1])); // play device
        CATCH_ERROR(_pDevices[0]->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void **)(&_pAudioClients[0])));
        CATCH_ERROR(_pDevices[1]->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void **)(&_pAudioClients[1])));
        set_wave_format(0);
        set_wave_format(1);
        initialize_buffer(0);
        initialize_buffer(1);
        start_core_process_exclusive_mode();
    }

    ~CoreProcess() {
        safe_release(_pEnumerator);
        safe_release(_pDevices[0]);
        safe_release(_pDevices[1]);
        safe_release(_pAudioClients[0]);
        safe_release(_pAudioClients[1]);
        if (_pWaveFormats[0]) {
            CoTaskMemFree(_pWaveFormats[0]);
            _pWaveFormats[0] = nullptr;
        }
        if (_pWaveFormats[1]) {
            CoTaskMemFree(_pWaveFormats[1]);
            _pWaveFormats[1] = nullptr;
        }
        safe_release(_pCaptureClient);
        safe_release(_pRenderClient);
        if (_waitArray[0]) {
            CloseHandle(_waitArray[0]);
            _waitArray[0] = nullptr;
        }
        CoUninitialize();
    }

private:

    inline void create_render_audio_client() noexcept {
        if (!_pDevices[1]) {
            WRITE_ERROR("_pRenderDev hasn't been created!")
        }
        else if (_pAudioClients[1]) {
            WRITE_ERROR("_pRenderAudioClient has been created!")
        }
        else {
            MyErrorCode error = _pDevices[1]->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&_pAudioClients[1]));
            ERROR_HANDLER(error)
        }
    }
    inline void set_wave_format(int index) noexcept {
        auto error = S_OK;
        WAVEFORMATEX *pWaveFormat = _pWaveFormats[index];
        WAVEFORMATEX *pClosest = nullptr;
        IAudioClient *pAudioClient = _pAudioClients[index];
        const char *pModeName = nullptr;
        AUDCLNT_SHAREMODE mode = _shareMode[index];

        {
            CATCH_ERROR(pAudioClient->GetMixFormat(&pWaveFormat)); // malloc space for *pWaveFormat

            pWaveFormat->wFormatTag = 1;
            pWaveFormat->nChannels = channel;
            pWaveFormat->nSamplesPerSec = sampleRate;
            pWaveFormat->nAvgBytesPerSec = sampleRate * channel * bitWidth / CHAR_BIT;
            pWaveFormat->nBlockAlign = channel * bitWidth / 8;
            pWaveFormat->wBitsPerSample = bitWidth;
            pWaveFormat->cbSize = 0;
            
            switch (pAudioClient->IsFormatSupported(mode, pWaveFormat, &pClosest)) {
                case S_OK:
                    break;
                case S_FALSE:
                    if (mode == AUDCLNT_SHAREMODE_EXCLUSIVE)
                        throw std::runtime_error("Wave format not supported");
                    *pWaveFormat = *pClosest;
                    CoTaskMemFree(pClosest);
                    pClosest = nullptr;
                    break;
                default:
                    throw std::runtime_error("Wave format not supported");
            }
            _pWaveFormats[index] = pWaveFormat;
            std::cout << _pWaveFormats[index] << std::endl;
        }
    }

    inline void initialize_buffer(int index) noexcept {
        IAudioClient *pAudioClient = _pAudioClients[index];
        WAVEFORMATEX *pWaveFormat = _pWaveFormats[index];
        MyErrorCode error = S_OK;
        bool shareMode = _shareMode[index];
        REFERENCE_TIME defaultDevicePeriod;
        REFERENCE_TIME minDevicePeriod;


        error = pAudioClient->GetDevicePeriod(&defaultDevicePeriod, &minDevicePeriod);
        ERROR_HANDLER(error)
        std::cout << "Default Device Period is " << defaultDevicePeriod / referenceTimePerMillisec << "ms\n";
        std::cout << "Minimum Device Period is " << minDevicePeriod / referenceTimePerMillisec << "ms\n";

        if (shareMode == AUDCLNT_SHAREMODE_SHARED) {
            error = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, minDevicePeriod, 0, pWaveFormat, nullptr);
        } else {
            error = pAudioClient->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, minDevicePeriod, minDevicePeriod, pWaveFormat, nullptr);
        }
        ERROR_HANDLER(error)

        _processDuration[index] = minDevicePeriod;

        REFERENCE_TIME streamLatency;
        UINT32 bufferFrameSize;

        error = pAudioClient->GetStreamLatency(&streamLatency);
        ERROR_HANDLER(error)
            std::cout << "The stream latency is " << streamLatency / referenceTimePerMillisec << "ms\n";

        error = pAudioClient->GetBufferSize(&bufferFrameSize);
        ERROR_HANDLER(error)
            std::cout << "The buffer has " << bufferFrameSize << "frames\n";
        std::cout << "The size of each frame is " << pWaveFormat->wBitsPerSample * pWaveFormat->nChannels / CHAR_BIT << "bytes\n";

        HANDLE hAudioReadyEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

        if (!hAudioReadyEvent) {
            WRITE_ERROR("Cannot create " << "audio ready event")
        }

        error = pAudioClient->SetEventHandle(hAudioReadyEvent);

        ERROR_HANDLER(error)

            if (index) {
                error = pAudioClient->GetService(IID_IAudioRenderClient, reinterpret_cast<void **>(&_pRenderClient));
            }
            else {
                error = pAudioClient->GetService(IID_IAudioCaptureClient, reinterpret_cast<void **>(&_pCaptureClient));
            }
        ERROR_HANDLER(error)

            _waitArray[index] = hAudioReadyEvent;

        if (index) {
            _callbackHandler->set_render_wave_format(_pWaveFormats[1]);
            _callbackHandler->about_to_render();
        }
        else {
            _callbackHandler->set_capture_wave_format(_pWaveFormats[0]);
            _callbackHandler->about_to_capture();
        }
        puts("Initialize buffer done.");
    }

    inline void start_core_process_exclusive_mode() noexcept {
        if (!_pAudioClients[0]) {
            WRITE_ERROR("capture audio client hasn't been created!")
        }
        else if (!_pAudioClients[1]) {
            WRITE_ERROR("render audio client hasn't been created!")
        }
        else if (!_pCaptureClient) {
            WRITE_ERROR("_pCaptureClient hasn't been created!")
        }
        else if (!_pRenderClient) {
            WRITE_ERROR("_pRenderClient hasn't been created!")
        }

        bool stillPlaying = true;
        DWORD waitResults;
        MyErrorCode error = S_OK;
        BYTE *pData = nullptr;
        UINT32 numFramesAvailable = 0;
        UINT32 renderMaxFrames = 0;
        bool bDone = 0;
        DWORD flags;
        HANDLE hTask = nullptr;
        DWORD taskIndex = 0;

        hTask = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
        if (!hTask) {
            WRITE_ERROR("boost the thread priority!")
        }

        error = _pAudioClients[1]->GetBufferSize(&renderMaxFrames);
        ERROR_HANDLER(error)

            error = _pRenderClient->GetBuffer(renderMaxFrames, &pData);
        ERROR_HANDLER(error)

            _callbackHandler->put_first_played_frame_data(renderMaxFrames, pData);

        error = _pRenderClient->ReleaseBuffer(renderMaxFrames, 0);
        // constexpr int error1 = AUDCLNT_E_INVALID_SIZE;
        // constexpr int error2 = AUDCLNT_E_BUFFER_SIZE_ERROR;
        // constexpr int error3 = AUDCLNT_E_OUT_OF_ORDER;
        // constexpr int error4 = AUDCLNT_E_DEVICE_INVALIDATED;
        // constexpr int error5 = AUDCLNT_E_SERVICE_NOT_RUNNING;
        // constexpr int error6 = E_INVALIDARG;
        ERROR_HANDLER(error)

            error = _pAudioClients[0]->Start();
        ERROR_HANDLER(error)
            error = _pAudioClients[1]->Start();
        ERROR_HANDLER(error)


            while (stillPlaying) {
                waitResults = WaitForMultipleObjects(2, _waitArray, FALSE, INFINITE);

                switch (waitResults) {
                    case WAIT_OBJECT_0 + 0:
                        //capture process
                        error = _pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);

                        ERROR_HANDLER(error)

                            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                                pData = nullptr;
                            }
                            else if (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR) {
                                WRITE_ERROR("AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR")
                            }

                        _callbackHandler->receive_captured_data(pData, numFramesAvailable, &stillPlaying);

                        error = _pCaptureClient->ReleaseBuffer(numFramesAvailable);

                        ERROR_HANDLER(error)
                            break;

                    case WAIT_OBJECT_0 + 1:
                        //render process
                        error = _pRenderClient->GetBuffer(renderMaxFrames, &pData);
                        ERROR_HANDLER(error)

                            _callbackHandler->put_to_be_played_data(renderMaxFrames, pData, &bDone);

                        error = _pRenderClient->ReleaseBuffer(renderMaxFrames, 0);
                }

                if (bDone) stillPlaying = false;
            }

        error = _pAudioClients[0]->Stop();
        ERROR_HANDLER(error)

            error = _pAudioClients[1]->Stop();
        ERROR_HANDLER(error)

            if (hTask)
                AvRevertMmThreadCharacteristics(hTask);
    }
};

#endif //CORE_PROCESS_HPP

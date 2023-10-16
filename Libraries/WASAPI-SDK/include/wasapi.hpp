#pragma once

#include <initguid.h>
#include "audio_io_project_base.hpp"

#include <memory>
#include <avrt.h>
#include <minwindef.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <iomanip>
#include <cstdint>
#include <iostream>
#include <sstream>


template<typename T>
inline void format_output_pair(std::ostream &ostream, const char *str, T value, std::size_t len1, std::size_t len2) {
    ostream << std::setw(len1) << std::setiosflags(std::ios::left) << std::setfill(' ') << str << std::setw(len2) << std::setiosflags(std::ios::left) << std::setfill(' ') << value << std::endl;
}

std::ostream &operator<<(std::ostream &ostream, const WAVEFORMATEX *wfx) {
    format_output_pair(ostream, "wFormatTag", wfx->wFormatTag, 20, 10);
    format_output_pair(ostream, "nChannels", wfx->nChannels, 20, 10);
    format_output_pair(ostream, "nSamplesPerSec", wfx->nSamplesPerSec, 20, 10);
    format_output_pair(ostream, "nAvgBytesPerSec", wfx->nAvgBytesPerSec, 20, 10);
    format_output_pair(ostream, "nBlockAlign", wfx->nBlockAlign, 20, 10);
    format_output_pair(ostream, "wBitsPerSample", wfx->wBitsPerSample, 20, 10);
    format_output_pair(ostream, "cbSize", wfx->cbSize, 20, 10);
    return ostream;
}


namespace WASAPI {

    class Exception : public std::exception {
        std::string msg;
    public:
        HRESULT code;
        Exception(HRESULT code, std::string file, int line) : code { code } {
            std::stringstream ss;
            ss << "0x" << std::hex << code << std::dec << " at " << file << ":" << line;
            msg = ss.str();
        }
        const char *what() const noexcept override { return msg.c_str(); }
    };


    #define CATCH_ERROR(API) \
        do { \
            auto result = (API); \
            if (result != S_OK) { \
                throw Exception(result, __FILE__, __LINE__); \
            } \
        } while (0)



    struct COMInitializer {
        COMInitializer() { CATCH_ERROR(CoInitializeEx(nullptr, 0)); }
        ~COMInitializer() { CoUninitialize(); }
    } comInitializer;

    template<typename T>
    class Ptr {
    protected:
        T *p;
    public:
        Ptr(T *p = nullptr) : p { p } { }
        auto get() noexcept { return p; }
        auto &operator=(auto &&o) noexcept { return std::swap(p, o.p), *this; }
        auto operator->() noexcept { return p; }
        auto &operator*() noexcept { return *p; }
        auto operator&() noexcept { return &p; }
        operator bool() noexcept { return p; }
    };

    template<typename T>
    class UniquePtr : public Ptr<T> {
    public:
        using Ptr<T>::Ptr;
        using Ptr<T>::operator=;
        UniquePtr(const UniquePtr &) = delete;
        auto &operator=(auto &) = delete;
        auto &operator=(const auto &) = delete;
    };

    template<typename T>
        requires std::is_base_of_v<IUnknown, T>
    class Interface : public Ptr<T> {
        friend class Ptr<T>;
    protected:
        using Ptr<T>::p;
    private:
        void decrease() noexcept { if (p) p->Release(); }
        void increase() noexcept { if (p) p->AddRef(); }
    public:
        ~Interface() noexcept { decrease(); }
        using Ptr<T>::Ptr;
        using Ptr<T>::operator=;
        auto &operator=(const Interface<T> &o) noexcept { return increase(), *this = Interface(p); }
    };

    class Handle : public UniquePtr<void> {
    public:
        Handle() = default;
        Handle(DWORD dwDesiredAccess) : UniquePtr<void> { CreateEventEx(nullptr, nullptr, 0, dwDesiredAccess) } { }
        ~Handle() { CloseHandle(p); }
        using UniquePtr<void>::operator=;
        auto &operator=(const Handle &o) = delete;
    };

    class WaveFormat : public UniquePtr<WAVEFORMATEX> {
    public:
        ~WaveFormat() { CoTaskMemFree(p); }
        using UniquePtr<WAVEFORMATEX>::UniquePtr;
        using UniquePtr<WAVEFORMATEX>::operator=;
        auto &operator=(const WaveFormat &o) = delete;
    };

    class PropertyStore : public Interface<IPropertyStore> {
        friend class Property;
    public:
        using Interface<IPropertyStore>::Interface;

        class Property {
            PropertyStore *pStore = nullptr;
            PROPERTYKEY key;
            PROPVARIANT value;
            friend PropertyStore;
            Property(PropertyStore *pStore, PROPERTYKEY key, PROPVARIANT value) : pStore { pStore }, key { key }, value { value } { }
        public:
            Property(PROPVARIANT value) : value { value } { }
            ~Property() { CATCH_ERROR(PropVariantClear(&value)); }
            auto operator=(REFPROPVARIANT propvarIn) & { return value = propvarIn; }
            auto operator=(REFPROPVARIANT propvarIn) && { CATCH_ERROR(pStore->p->SetValue(key, propvarIn)); return value = propvarIn; }
        };

        auto operator[](PROPERTYKEY key) {
            PROPVARIANT value;
            PropVariantInit(&value);
            CATCH_ERROR(p->GetValue(key, &value));
            return Property(this, key, value);
        }

    };

    class AudioRenderClient : public Interface<IAudioRenderClient> {
    public:
        using Interface<IAudioRenderClient>::Interface;

        auto get_buffer(UINT32 size) {
            BYTE *buffer;
            CATCH_ERROR(p->GetBuffer(size, &buffer));
            return buffer;
        }

        auto release_buffer(UINT32 size) {
            CATCH_ERROR(p->ReleaseBuffer(size, 0));
        }

    };

    class AudioCaptureClient : public Interface<IAudioCaptureClient> {
    public:
        using Interface<IAudioCaptureClient>::Interface;

        auto get_buffer() {
            BYTE *buffer; UINT32 size; DWORD flags;
            CATCH_ERROR(p->GetBuffer(&buffer, &size, &flags, nullptr, nullptr));
            return std::tuple<BYTE *, UINT32, DWORD> { buffer, size, flags };
        }

        auto release_buffer(UINT32 size) {
            CATCH_ERROR(p->ReleaseBuffer(size));
        }
    };

    class AudioClient : public Interface<IAudioClient> {
    public:
        using Interface<IAudioClient>::Interface;

        auto get_mix_format() {
            WAVEFORMATEX *pFormat;
            CATCH_ERROR(p->GetMixFormat(&pFormat));
            return WaveFormat(pFormat);
        }

        bool is_format_supported(
            AUDCLNT_SHAREMODE shareMode,
            WaveFormat &pFormat
        ) {
            WaveFormat pClosest;
            switch (p->IsFormatSupported(shareMode, pFormat.get(), &pClosest)) {
                case S_OK:
                    return true;
                case S_FALSE:
                    if (shareMode == AUDCLNT_SHAREMODE_EXCLUSIVE)
                        return false;
                    *pFormat = *pClosest;
                    return true;
                default:
                    return false;
            }
        }

        std::pair<REFERENCE_TIME, REFERENCE_TIME> get_device_period() {
            REFERENCE_TIME defaultDevicePeriod;
            REFERENCE_TIME minDevicePeriod;
            CATCH_ERROR(p->GetDevicePeriod(&defaultDevicePeriod, &minDevicePeriod));
            return { defaultDevicePeriod, minDevicePeriod };
        }

        void initialize(
            AUDCLNT_SHAREMODE shareMode,
            REFERENCE_TIME devicePeriod,
            WaveFormat &waveFormat
        ) {
            CATCH_ERROR(p->Initialize(
                shareMode,
                AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                devicePeriod,
                (shareMode == AUDCLNT_SHAREMODE_SHARED ? 0 : devicePeriod),
                waveFormat.get(),
                nullptr
            ));
        }

        auto test_min_device_period(
            AUDCLNT_SHAREMODE shareMode,
            WaveFormat &waveFormat
        ) {
            REFERENCE_TIME realMinPeriod;
            for(std::size_t i = 0 ; i < 1000 ; ++i) {
                realMinPeriod = 10000000. / waveFormat->nSamplesPerSec * i;
                auto code = p->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, realMinPeriod, realMinPeriod, waveFormat.get(), nullptr);
                if(SUCCEEDED(code)) {
                    std::cout << "Find a minimum device period in exclusive mode successfully!\n";
                    std::cout << "which is " << realMinPeriod / 10000. << "ms\n";
                    break;
                }
            }
            return realMinPeriod;
        }

        auto get_stream_latency() {
            REFERENCE_TIME streamLatency;
            CATCH_ERROR(p->GetStreamLatency(&streamLatency));
            return streamLatency;
        }

        auto get_buffer_size() {
            UINT32 bufferFrameSize;
            CATCH_ERROR(p->GetBufferSize(&bufferFrameSize));
            return bufferFrameSize;
        }

        auto set_event_handle(Handle &handle) {
            CATCH_ERROR(p->SetEventHandle(handle.get()));
        }

        template<class S>
        auto get_service() {
            S *client;
            CATCH_ERROR(p->GetService(__uuidof(S), (void **)&client));
            return client;
        }

        auto get_capture_client() {
            return AudioCaptureClient(get_service<IAudioCaptureClient>());
        }

        auto get_render_client() {
            return AudioRenderClient(get_service<IAudioRenderClient>());
        }

        auto start() {
            CATCH_ERROR(p->Start());
        }

        auto stop() {
            CATCH_ERROR(p->Stop());
        }

        auto reset() {
            CATCH_ERROR(p->Reset());
        }

    };

    class Device : public Interface<IMMDevice> {
    public:
        using Interface<IMMDevice>::Interface;

        enum class State {
            active = DEVICE_STATE_ACTIVE,
            disabled = DEVICE_STATE_DISABLED,
            not_present = DEVICE_STATE_NOTPRESENT,
            unplugged = DEVICE_STATE_UNPLUGGED,
            all = DEVICE_STATEMASK_ALL
        };

        /* IMMDevice::Activate */
        auto get_client(DWORD dwClsCtx = CLSCTX_ALL, PROPVARIANT *pActivationParams = nullptr) {
            IAudioClient *pAudioClient = nullptr;
            CATCH_ERROR(p->Activate(IID_IAudioClient, dwClsCtx, pActivationParams, (void **)&pAudioClient));
            return AudioClient(pAudioClient);
        }

        /* IMMDevice::OpenPropertyStore */
        auto get_property_store() {
            IPropertyStore *pPropertyStore;
            CATCH_ERROR(p->OpenPropertyStore(STGM_READ, &pPropertyStore));
            return PropertyStore(pPropertyStore);
        }

    };

    class DeviceCollection : public Interface<IMMDeviceCollection> {
    public:
        using Interface<IMMDeviceCollection>::Interface;

        auto operator[](UINT index) {
            IMMDevice *pDevice = nullptr;
            CATCH_ERROR(p->Item(index, &pDevice));
            return Device(pDevice);
        }

        auto size() {
            UINT count;
            CATCH_ERROR(p->GetCount(&count));
            return count;
        }
    };

    class DeviceEnumerator : public Interface<IMMDeviceEnumerator> {
    public:
        DeviceEnumerator() {
            CATCH_ERROR(CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, (LPVOID *)(&p)));
        }

        /* IMMDeviceEnumerator::GetDevice */
        auto get_default_device(EDataFlow flow, ERole role = eConsole) {
            IMMDevice *pDevice = nullptr;
            CATCH_ERROR(p->GetDefaultAudioEndpoint(flow, role, &pDevice));
            return Device(pDevice);
        }

        /* IMMDeviceEnumerator::EnumAudioEndpoints */
        auto get_device_collection(EDataFlow flow, Device::State dwStateMask = Device::State::all) {
            IMMDeviceCollection *pCollection = nullptr;
            CATCH_ERROR(p->EnumAudioEndpoints(flow, (DWORD)dwStateMask, &pCollection));
            return DeviceCollection(pCollection);
        }

    };

}


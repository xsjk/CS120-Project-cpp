#ifndef CONSTANT_HPP
#define CONSTANT_HPP

#include <chrono>
#include <cstddef>
#include <mmdeviceapi.h>
#include <audioclient.h>

constexpr std::chrono::nanoseconds referenceTime = std::chrono::nanoseconds{100};

constexpr std::size_t referenceTimePerSec = 
    static_cast<std::chrono::nanoseconds>(std::chrono::seconds{1}).count() / referenceTime.count();
constexpr std::size_t referenceTimePerMillisec = 
    static_cast<std::chrono::nanoseconds>(std::chrono::milliseconds{1}).count() / referenceTime.count();

constexpr CLSID CLSID_MMDeviceEnumerator    =   __uuidof(MMDeviceEnumerator);
constexpr IID   IID_IMMDeviceEnumerator     =   __uuidof(IMMDeviceEnumerator);
constexpr IID   IID_IAudioClient            =   __uuidof(IAudioClient);
constexpr IID   IID_IAudioCaptureClient     =   __uuidof(IAudioCaptureClient);
constexpr IID   IID_IAudioRenderClient      =   __uuidof(IAudioRenderClient);


#endif //CONSTANT_HPP
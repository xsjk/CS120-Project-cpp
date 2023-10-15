#include <iostream>
#include <cstring>
#include <chrono>
#include <cmath>
#include <memory>
#include <utility>
#include <fstream>
#include <vector>
#include <mutex>
#include <algorithm>
#include <queue>

#include "audio_io_project_base.hpp"
#include "core_process.hpp"

class Test : public AudioIOProjectBase {
    std::ofstream file;
    static constexpr std::size_t channel = 2;
    static constexpr std::size_t sampleRate = 48'000;
    static constexpr std::size_t bitWidth = 16;
    static constexpr std::size_t frameBytes = channel * bitWidth / CHAR_BIT ; 

    bool _isCaptureDone;
    std::mutex _mtx;

    struct chunk_t {
        int _[4];
    };
    std::queue<chunk_t> _data;

    float record_t = 0;
    float play_t = 0;

public:
    Test() {
        file.open("test.txt");
    }
    
    void about_to_render() noexcept override {
        std::cout << "Render wave format is\n" << get_current_render_wave_format() << std::endl;
    }

    void about_to_capture() noexcept override {
        std::cout << "Capture wave format is\n" << get_current_capture_wave_format() << std::endl;
    }

    void receive_captured_data(BYTE* pBuffer, std::size_t availableFrameCnt, bool* stillPlaying) noexcept override {
        // std::cout << record_t << " receive_captured_data " << availableFrameCnt << std::endl;
        for (auto i = 0; i < availableFrameCnt * frameBytes / sizeof(int); i++) {
            file << ((int*)pBuffer)[i] << std::endl;
        }

        std::lock_guard<std::mutex> lock(_mtx);
        for (auto i = 0; i < availableFrameCnt * frameBytes / sizeof(chunk_t); i++) {
            _data.push(((chunk_t*)pBuffer)[i]);
        }
        
        if ((record_t += availableFrameCnt * 1.0 / sampleRate) > 5) {
            std::cout << "Time to stop playing 1" << std::endl;
            *stillPlaying = false;
        }
    }

    void put_to_be_played_data(std::size_t availableFrameCnt, BYTE* pBuffer, bool* bDone) noexcept override {
        // std::cout << play_t << " put_to_be_played_data " << availableFrameCnt << std::endl;

        std::lock_guard<std::mutex> lock(_mtx);
        for (auto i = 0; i < availableFrameCnt * frameBytes / sizeof(chunk_t); i++) {
            if (_data.empty()) {
                std::cout << "No more data to play" << std::endl;
                break;
            }
            ((chunk_t*)pBuffer)[i] = _data.front();
            _data.pop();
        }
        
        if ((play_t += availableFrameCnt * 1.0 / sampleRate) > 5) {
            std::cout << "Time to stop playing 2" << std::endl;
            *bDone = true;
        }
    }

    void put_first_played_frame_data(std::size_t availableFrameCnt, BYTE* pBuffer) noexcept override {
        std::cout << "put_first_played_frame_data" << std::endl;
    }

    void set_expected_capture_wave_format(WAVEFORMATEX *pCaptureWaveFormat) noexcept override {
        pCaptureWaveFormat->wFormatTag = 1;
        pCaptureWaveFormat->nSamplesPerSec = sampleRate;
        pCaptureWaveFormat->wBitsPerSample = bitWidth;
        pCaptureWaveFormat->nChannels = channel;
        pCaptureWaveFormat->nAvgBytesPerSec = sampleRate * channel * bitWidth / CHAR_BIT;
        pCaptureWaveFormat->nBlockAlign = channel * bitWidth / 8;
        pCaptureWaveFormat->cbSize = 0;
    }

    void set_excepted_render_wave_format(WAVEFORMATEX *pRenderWaveFormat) noexcept override {
        pRenderWaveFormat->wFormatTag = 1;
        pRenderWaveFormat->nSamplesPerSec = sampleRate;
        pRenderWaveFormat->wBitsPerSample = bitWidth;
        pRenderWaveFormat->nChannels = channel;
        pRenderWaveFormat->nAvgBytesPerSec = sampleRate * channel * bitWidth / CHAR_BIT;
        pRenderWaveFormat->nBlockAlign = channel * bitWidth / 8;
        pRenderWaveFormat->cbSize = 0;
    }

};




int main() {
    std::shared_ptr<AudioIOProjectBase> p = std::make_shared<Test>();
    std::shared_ptr<CoreProcess> pProcess = std::make_shared<CoreProcess>(
        std::move(p), 
        AUDCLNT_SHAREMODE::AUDCLNT_SHAREMODE_EXCLUSIVE, 
        AUDCLNT_SHAREMODE::AUDCLNT_SHAREMODE_EXCLUSIVE
    );
    pProcess->core_process();
    p = std::move(pProcess->get_project_base());
    return 0;
}
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
#include <limits>
#include <valarray>

#include "wasapidevice.hpp"


class SineWave : public WASAPIIOHandler {
    std::ofstream file;
    static constexpr std::size_t channel = 2;
    static constexpr std::size_t sampleRate = 48'000;
    static constexpr std::size_t bitWidth = 16;
    static constexpr std::size_t frameBytes = channel * bitWidth / CHAR_BIT;

    float phase = 0;
public:
    void inputCallback(BYTE *pBuffer, std::size_t availableFrameCnt) noexcept override { }

    void outputCallback(std::size_t availableFrameCnt, BYTE *pBuffer) noexcept override {
        using T = short;
        T *p = (T *)pBuffer;
        constexpr auto a = std::numeric_limits<T>::max();
        for (auto i = 0; i < availableFrameCnt * frameBytes / sizeof(T); i += 2) {
            phase += 2 * std::numbers::pi * 440 / sampleRate;
            p[i] = std::sin(phase) * a;
            p[i + 1] = 0;
        }
    }

};


class Recorder : public WASAPIIOHandler {
    std::ofstream file;
    static constexpr std::size_t channel = 2;
    static constexpr std::size_t sampleRate = 48'000;
    static constexpr std::size_t bitWidth = 16;
    static constexpr std::size_t frameBytes = channel * bitWidth / CHAR_BIT;

    struct chunk_t { BYTE _[32]; };
    std::queue<chunk_t> data;

public:

    Recorder() {
        file.open("test.txt");
    }

    void inputCallback(BYTE *pBuffer, std::size_t availableFrameCnt) noexcept override {
        using T = short;
        T *p = (T *)pBuffer;
        for (auto i = 0; i < availableFrameCnt * frameBytes / sizeof(T); i+=2) {
            file << p[i] << std::endl;
        }

        for (auto i = 0; i < availableFrameCnt * frameBytes / sizeof(chunk_t); i++) {
            data.push(((chunk_t *)pBuffer)[i]);
        }

    }

    void outputCallback(std::size_t availableFrameCnt, BYTE *pBuffer) noexcept override {
        for (auto i = 0; i < availableFrameCnt * frameBytes / sizeof(chunk_t); i++) {
            if (data.empty()) {
                std::cout << "No more data to play" << std::endl;
                break;
            }
            ((chunk_t *)pBuffer)[i] = data.front();
            data.pop();
        }

    }

};




int main() {
    auto p = std::make_shared<Recorder>();
    WASAPIDevice wasapi;
    wasapi.open(2, 48000, 16);
    wasapi.start(p);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}
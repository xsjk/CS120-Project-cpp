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
#include "wasapidevice.hpp"

class Test : public WASAPIIOHandler {
    std::ofstream file;
    static constexpr std::size_t channel = 2;
    static constexpr std::size_t sampleRate = 48'000;
    static constexpr std::size_t bitWidth = 16;
    static constexpr std::size_t frameBytes = channel * bitWidth / CHAR_BIT ; 

    struct chunk_t { BYTE _[16]; };
    std::queue<chunk_t> data;

public:
    Test() {
        file.open("test.txt");
    }

    void inputCallback(BYTE* pBuffer, std::size_t availableFrameCnt) noexcept override {
        for (auto i = 0; i < availableFrameCnt * frameBytes / sizeof(int); i++) {
            file << ((int*)pBuffer)[i] << std::endl;
        }

        for (auto i = 0; i < availableFrameCnt * frameBytes / sizeof(chunk_t); i++) {
            data.push(((chunk_t*)pBuffer)[i]);
        }
        
    }

    void outputCallback(std::size_t availableFrameCnt, BYTE* pBuffer) noexcept override {

        for (auto i = 0; i < availableFrameCnt * frameBytes / sizeof(chunk_t); i++) {
            if (data.empty()) {
                std::cout << "No more data to play" << std::endl;
                break;
            }
            ((chunk_t*)pBuffer)[i] = data.front();
            data.pop();
        }

    }

};




int main() {
    std::shared_ptr<WASAPIIOHandler> p = std::make_shared<Test>();
    WASAPIDevice wasapi;
    wasapi.open(2, 48000, 16);
    wasapi.start(p);
    wasapi.mainloop();
    return 0;
}
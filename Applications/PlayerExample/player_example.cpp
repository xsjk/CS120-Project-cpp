// Audio Stream Input/Output library
#include <iostream>
#include <cmath>
#include <string>
#include <numbers>
#include "asiosys.h"
#include "asiodrvr.h"
#include "asio.h"
#include "Windows.h"
#include "asiodrivers.h"

#define CATCH_ERROR(API) \
    do { \
        ASIOError result = API; \
        if (result != ASE_OK) { \
            std::cerr << #API << " failed with error code " << result << std::endl; \
            throw std::runtime_error(#API " failed at " __FILE__ ":" + std::to_string(__LINE__)); \
        } \
    } while (0)


ASIOBufferInfo bufInfo[4];

static void convertFromFloat (const float* src, int* dest, int n) noexcept {
    constexpr double maxVal = 0x7fffffff;
    while (--n >= 0) {
        auto val = maxVal * *src++;
        *dest++ = round(val < -maxVal ? -maxVal : (val > maxVal ? maxVal : val));
    }
}
static void convertToFloat (const int* src, float* dest, int n) noexcept {
    constexpr double g = 1. / 0x7fffffff;
    while (--n >= 0) *dest++ = g * *src++;
}

void buffer_switch(long bufferIndex, ASIOBool) {
        
    static float outBuffers[2][512];
    static float inBuffers[2][512];
    for (int i = 0; i < 2; ++i)
        convertToFloat ((int*)bufInfo[i].buffers[bufferIndex], inBuffers[i], 512);
    {
        constexpr float dphase = 2 * std::numbers::pi * 440. / 44100.;
        static float phase = 0;
        for (int j = 0; j < 512; ++j)
            outBuffers[0][j] = outBuffers[1][j] = sinf(phase += dphase);
    }
    for (int i = 0; i < 2; ++i)
        convertFromFloat (outBuffers[i], (int*)bufInfo[2 + i].buffers[bufferIndex], 512);
}

int main() {
    AsioDrivers drivers;
    ASIODriverInfo driver_info;
    ASIOCallbacks callbacks = { .bufferSwitch = buffer_switch};

    drivers.loadDriver("ASIO4ALL v2");
    CATCH_ERROR(ASIOInit(&driver_info));
    for (int i = 0; i < 4; i++) {
        bufInfo[i].channelNum = i % 2;
        bufInfo[i].isInput = i < 2;
    }

    CATCH_ERROR(ASIOCreateBuffers(bufInfo, 4, 512, &callbacks));
    CATCH_ERROR(ASIOStart());

    Sleep(1000);
    return 0;
}
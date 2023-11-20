#pragma once

#define __JUCE
#define LOG

#if defined(__WASAPI)
    #include "wasapidevice.hpp"
    using namespace WASAPI;
#elif defined(__ASIO)
    #include "asiodevice.h"
    using namespace ASIO;
#elif defined(__JUCE)
    #include "jucedevice.h"
    using namespace JUCE;
#else
    #error "No audio API selected"
#endif



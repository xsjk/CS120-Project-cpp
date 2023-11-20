#pragma once

#define __ASIO
#define LOG

#if defined(__WASAPI)
    #include "wasapidevice.hpp"
    using namespace WASAPI;
#elif defined(__ASIO)
    #include "asiodevice.h"
    using namespace ASIO;
#elif defined(__FAKE)
    #include "fakedevice.hpp"
    using namespace FAKE;
#else
    #error "No audio API selected"
#endif



#ifndef UTILS_HPP
#define UTULS_HPP

#include "error_code.hpp"

#include <cstdlib>
#include <iostream>
#include <mmsystem.h>
#include <iomanip>

template<typename T>
inline void safe_release(T*& refPtr) {
    if(refPtr) {
        refPtr->Release();
        refPtr = nullptr;
    }
}

inline void error_handler(MyErrorCode error, const char* file, int line) {
    if(error) {
        std::cerr << "Runtime error in:\n" << file << ":" << line << "\n" << "The error code is " << static_cast<HRESULT>(error) << std::endl;
        exit(1);
    }
}

#define ERROR_HANDLER(errorCode)\
    error_handler(errorCode, __FILE__, __LINE__);

#define WRITE_ERROR(what) {\
    std::cerr << "Runtime Error happens in file " << __FILE__ << ":" << __LINE__ << "\n" << what << std::endl;\
}

template<typename T>
inline void format_output_pair(std::ostream& ostream, const char* str, T value, std::size_t len1, std::size_t len2) {
  ostream << std::setw(len1) << std::setiosflags(std::ios::left) << std::setfill(' ') << str << std::setw(len2) << std::setiosflags(std::ios::left) << std::setfill(' ') << value << std::endl;
}

std::ostream& operator<<(std::ostream& ostream, const WAVEFORMATEX* wfx) {
  format_output_pair(ostream, "wFormatTag", wfx->wFormatTag, 20, 10);
  format_output_pair(ostream, "nChannels", wfx->nChannels, 20, 10);
  format_output_pair(ostream, "nSamplesPerSec", wfx->nSamplesPerSec, 20, 10);
  format_output_pair(ostream, "nAvgBytesPerSec", wfx->nAvgBytesPerSec, 20, 10);
  format_output_pair(ostream, "nBlockAlign", wfx->nBlockAlign, 20, 10);
  format_output_pair(ostream, "wBitsPerSample", wfx->wBitsPerSample, 20, 10);
  format_output_pair(ostream, "cbSize", wfx->cbSize, 20, 10);
  return ostream;
}


#endif  //UTILS_HPP
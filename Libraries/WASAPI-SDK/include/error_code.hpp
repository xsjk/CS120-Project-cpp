#ifndef ERROR_CODE_HPP
#define ERROR_CODE_HPP

#include <winnt.h>
#include <iostream>
#include <iomanip>

struct MyErrorCode {
    HRESULT m_errorCode;

    constexpr MyErrorCode() : m_errorCode{0} {}
    
    constexpr MyErrorCode(HRESULT errorCode) : m_errorCode{errorCode} {}

    constexpr operator bool () const {
        return m_errorCode < 0;
    }

    constexpr operator HRESULT() const {
      return m_errorCode;
    }
};

inline std::ostream& operator<<(std::ostream& ostream, MyErrorCode errorCode) {
  return ostream << "0x" << std::setw(8) << std::setiosflags(std::ios::right) << std::setfill('0') << std::hex << errorCode.m_errorCode;
}

#endif //ERROR_CODE_HPP

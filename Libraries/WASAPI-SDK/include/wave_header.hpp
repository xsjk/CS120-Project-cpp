#ifndef WAVE_HEADER_HPP
#define WAVE_HEADER_HPP

#include <minwindef.h>

struct WAVEHEADER {
	DWORD   dwRiff;                     // "RIFF"
	DWORD   dwSize;                     // Size
	DWORD   dwWave;                     // "WAVE"
	DWORD   dwFmt;                      // "fmt "
	DWORD   dwFmtSize;                  // Wave Format Size
};

constexpr BYTE waveHeader[] =   {'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',0,0,0,0};

constexpr BYTE waveData[]   =   {'d','a','t','a'};

#endif //WAVE_HEADER_HPP
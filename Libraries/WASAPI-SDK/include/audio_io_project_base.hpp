#ifndef AUDIO_IO_PROJECT_BASE_HPP
#define AUDIO_IO_PROJECT_BASE_HPP

#include <cstddef>
#include <minwindef.h>
#include <windef.h>
#include <audioclient.h>

class AudioIOProjectBase {
  public:
    inline const WAVEFORMATEX *get_current_capture_wave_format() const noexcept {
        return _captureWaveFormat;
    }
    inline const WAVEFORMATEX *get_current_render_wave_format() const noexcept {
        return _renderWaveFormat;
    }
    inline void set_capture_wave_format(WAVEFORMATEX *waveFormat) noexcept {
      _captureWaveFormat = waveFormat;
    }
    inline void set_render_wave_format(WAVEFORMATEX *waveFormat) noexcept {
      _renderWaveFormat = waveFormat;
    }
    virtual void about_to_render()  noexcept    =   0;
    virtual void about_to_capture() noexcept    =   0;
    /**
     * @param flag As long as the `AudioIOProjectBase` object requires additional data, 
     * the `receive_captured_data` function outputs the value `FALSE` through its third parameter. 
     * When the `AudioIOProjectBase` object has all the data that it requires, 
     * the `receive_captured_data` function sets bDone to `TRUE`, 
     * which causes the program to exit the loop in process capture a stream.
    */
    virtual void receive_captured_data(BYTE* pBuffer, std::size_t availableFrameCnt, bool* stillPlaying) noexcept = 0;

    virtual void put_first_played_frame_data (std::size_t availableFrameCnt, BYTE* pBuffer) noexcept = 0;
    /**
     * @param flag As long as `put_to_be_played_data` succeeds in writing at least one frame of real data (not silence) to the specified buffer location, it outputs 0 through its third parameter, 
     * When `put_to_be_played_data` is out of data and cannot write even a single frame to the specified buffer location, 
     * it writes nothing to the buffer (not even silence), and it writes the value `AUDCLNT_BUFFERFLAGS_SILENT` to the flags variable. 
     * The flags variable conveys this value to the IAudioRenderClient::ReleaseBuffer method, which responds by filling the specified number of frames in the buffer with silence.
    */
    virtual void put_to_be_played_data(std::size_t availableFrameCnt, BYTE* pBuffer, bool* bDone) noexcept = 0;
    
    //these functions is only called by CoreProcess
    virtual void set_expected_capture_wave_format(WAVEFORMATEX *pCaptureWaveFormat) noexcept = 0;
    virtual void set_excepted_render_wave_format(WAVEFORMATEX *pRenderWaveFormat) noexcept = 0;
    virtual ~AudioIOProjectBase() {}
  private:
    //the space where these pointers point to is allocated by CoreProcess
    WAVEFORMATEX*   _captureWaveFormat  = nullptr;
    WAVEFORMATEX*   _renderWaveFormat   = nullptr;

};

#endif //AUDIO_IO_PROJECT_BASE_HPP
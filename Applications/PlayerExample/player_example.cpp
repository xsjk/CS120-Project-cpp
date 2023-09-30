// Audio Stream Input/Output library
#include <iostream>
#include <cmath>
#include "asiosys.h"
#include "asio.h"
#include "asiodrivers.h"
#include "Windows.h"

#define M_PI 3.14159265358979323846


#define CATCH_ERROR(API) \
    do { \
        ASIOError result = API; \
        if (result != ASE_OK) { \
            std::cerr << #API << " failed with error code " << result << std::endl; \
            throw std::runtime_error(#API " failed"); \
        } \
    } while (0)


ASIOBufferInfo input_buffer_info = { .isInput = ASIOTrue };
ASIOBufferInfo output_buffer_info = { .isInput = ASIOFalse };
ASIOChannelInfo input_channel_info;
ASIOChannelInfo output_channel_info;
long buffer_size;

// ASIO driver buffer switch function
void buffer_switch(long index, ASIOBool process_now) {

}


ASIOCallbacks asio_callbacks = {
    .bufferSwitch = buffer_switch,
};



void display_info() {

    // display information
    long num_input_channels, num_output_channels;
    CATCH_ERROR(ASIOGetChannels(&num_input_channels, &num_output_channels));
    std::cout << "Number of input channels: " << num_input_channels << std::endl;
    std::cout << "Number of output channels: " << num_output_channels << std::endl;

    long input_latency, output_latency;
    CATCH_ERROR(ASIOGetLatencies(&input_latency, &output_latency));
    std::cout << "Input latency: " << input_latency << " samples" << std::endl;
    std::cout << "Output latency: " << output_latency << " samples" << std::endl;

    long min_buffer_size, max_buffer_size, preferred_buffer_size, buffer_size_granularity;
    CATCH_ERROR(ASIOGetBufferSize(&min_buffer_size, &max_buffer_size, &preferred_buffer_size, &buffer_size_granularity));
    std::cout << "Minimum buffer size: " << min_buffer_size << " samples" << std::endl;
    std::cout << "Maximum buffer size: " << max_buffer_size << " samples" << std::endl;
    std::cout << "Preferred buffer size: " << preferred_buffer_size << " samples" << std::endl;
    std::cout << "Buffer size granularity: " << buffer_size_granularity << " samples" << std::endl;

    ASIOSampleRate sample_rate = 48000;
    CATCH_ERROR(ASIOCanSampleRate(sample_rate));
    CATCH_ERROR(ASIOGetSampleRate(&sample_rate));
    std::cout << "Current sample rate: " << sample_rate << " Hz" << std::endl;
    // CATCH_ERROR(ASIOSetSampleRate(sample_rate));

    ASIOClockSource clock_source;
    long num_clock_sources;
    CATCH_ERROR(ASIOGetClockSources(&clock_source, &num_clock_sources));
    std::cout << "Number of clock sources: " << num_clock_sources << std::endl;
    std::cout << "Clock source: " << clock_source.name << std::endl;

    ASIOSamples sample_position;
    ASIOTimeStamp sample_time_stamp;
    CATCH_ERROR(ASIOGetSamplePosition(&sample_position, &sample_time_stamp));
    std::cout << "Sample position: " << sample_position.hi << " " << sample_position.lo << std::endl;
    std::cout << "Sample time stamp: " << sample_time_stamp.hi << " " << sample_time_stamp.lo << std::endl;


    // CATCH_ERROR(ASIOControlPanel());
    // std::cout << "Control panel opened" << std::endl;

}
// Main function
int main() {

    AsioDrivers drivers;
    drivers.loadDriver("ASIO4ALL v2");

    // Initialize ASIO driver
    ASIODriverInfo asio_driver_info;
    CATCH_ERROR(ASIOInit(&asio_driver_info));

    CATCH_ERROR(ASIOCreateBuffers(&input_buffer_info, 1, 1024, &asio_callbacks));
    CATCH_ERROR(ASIOCreateBuffers(&output_buffer_info, 1, 1024, &asio_callbacks));
    
    display_info();

    // Start ASIO driver
    CATCH_ERROR(ASIOStart());
    
    // Wait 1s
    Sleep(1000);
    
    CATCH_ERROR(ASIODisposeBuffers());
    CATCH_ERROR(ASIOStop());

    return 0;
}
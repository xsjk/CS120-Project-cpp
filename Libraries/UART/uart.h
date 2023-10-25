#include "asio.h"
#include "asiodevice.h"
#include <iostream>
#include <mutex>
#include <cmath>
#include <vector>
#include <thread>
#include <queue>


class BitstreamDevice : public ASIO::Device {
    std::vector<bool> inputChannelDataBool;
    std::vector<bool> outputChannelDataBool;

public:
    void open();
    
protected:
    void audioDeviceIOCallback(const int *const *inputChannelData, int *const *outputChannelData) override;
    virtual void bitstreamCallback(const std::vector<bool> &inputChannelData, std::vector<bool> &outputChannelData) = 0;
    
};


enum class Parity {
    None = 'N',
    Even = 'E',
    Odd = 'O'
};


class UARTDevice : public BitstreamDevice {
    std::queue<bool> rxBuffer;
    std::queue<bool> txBuffer;
    std::mutex rxLock;
    std::mutex txLock;

    bool isOpen = false;

    int baudrate = 9600;
    struct FrameConfig {
        int data_bits = 8;
        Parity parity_type = Parity::None;
        int stop_bits = 1;
    } frameConfig;

    enum class State {
        Idle,
        StartBit,
        DataBits,
        ParityBit,
        StopBit
    };

    State outState = State::Idle;
    State inState = State::Idle;
    int outBitCounter = 0;
    int inBitCounter = 0;
    bool outParity = 0;
    bool inParity = 0;
    std::vector<bool> inBuffer;
    
public:
    UARTDevice();
    void open(int baudrate = 9600, int data_bits = 8, Parity parity_type = Parity::None, int stop_bits = 1);
    void close();
    void write(char data);
    bool available();
    char read();

protected:
    void bitstreamCallback(const std::vector<bool> &inputChannelData, std::vector<bool> &outputChannelData) override;
};
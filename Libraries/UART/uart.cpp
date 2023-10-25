#include "uart.h"
#include <iostream>

void BitstreamDevice::audioDeviceIOCallback(const int *const *inputChannelData, int *const *outputChannelData) {
    for (int j = 0; j < bufferSize; j++)
        inputChannelDataBool[j] = inputChannelData[0][j] > 0;
    bitstreamCallback(inputChannelDataBool, outputChannelDataBool);
    for (int j = 0; j < bufferSize; j++)
        outputChannelData[0][j] = outputChannelDataBool[j] ? 0x7fffffff : 0x80000000;
}

void BitstreamDevice::open() {
    ASIO::Device::open(1, 1, 44100);
    inputChannelDataBool.resize(bufferSize);
    outputChannelDataBool.resize(bufferSize);
}

UARTDevice::UARTDevice() {
    BitstreamDevice::open();
}

void UARTDevice::open(int baudrate, int data_bits, Parity parity_type, int stop_bits) {
    isOpen = true;
    frameConfig.data_bits = data_bits;
    frameConfig.parity_type = parity_type;
    frameConfig.stop_bits = stop_bits;
}

void UARTDevice::close() {
    isOpen = false;
}

void UARTDevice::write(char data) {
    std::lock_guard<std::mutex> lock(txLock);
    for (int i = 7; i >= 0; i--)
        txBuffer.push((data >> i) & 1);
}

bool UARTDevice::available() {
    std::lock_guard<std::mutex> lock(rxLock);
    return rxBuffer.size() >= 8;
}

char UARTDevice::read() {
    std::lock_guard<std::mutex> lock(rxLock);
    char data = 0;
    if (rxBuffer.size() >= 8) {
        for (int i = 7; i >= 0; i--) {
            data |= rxBuffer.front() << i;
            rxBuffer.pop();
        }
    }
    return data;
}

void UARTDevice::bitstreamCallback(const std::vector<bool> &in, std::vector<bool> &out) {
    if (isOpen) {
        // TX
        {
            int i = 0;
            while (i < out.size()) {
                switch (outState) {
                    case State::Idle:
                        if (txBuffer.size() >= frameConfig.data_bits)
                            outState = State::StartBit;
                        out[i++] = 1;
                        break;
                    case State::StartBit:
                        std::cout << "StartBit" << std::endl;
                        out[i++] = 0;
                        outState = State::DataBits;
                        break;
                    case State::DataBits:
                        std::cout << "DataBits " << txBuffer.size() << std::endl;
                        if (outBitCounter++ < frameConfig.data_bits) {
                            std::lock_guard<std::mutex> lock(txLock);
                            outParity ^= out[i++] = txBuffer.front();
                            txBuffer.pop();
                        } else {
                            outBitCounter = 0;
                            outState = State::ParityBit;
                        }
                        break;
                    case State::ParityBit:
                        std::cout << "ParityBit" << std::endl;
                        if (frameConfig.parity_type == Parity::Even)
                            out[i++] = outParity;
                        else if (frameConfig.parity_type == Parity::Odd)
                            out[i++] = !outParity;
                        outParity = 0;
                        outState = State::StopBit;
                        break;
                    case State::StopBit:
                        std::cout << "StopBit" << std::endl;
                        if (outBitCounter++ < frameConfig.stop_bits)
                            out[i++] = 1;
                        else {
                            outBitCounter = 0;
                            outState = State::Idle;
                        }
                        break;
                }
            }
        }
        
        // RX
        {
            int i = 0;
            while (i < in.size()) {
                switch (inState) {
                    case State::Idle:
                        if (in[i++] == 0)
                            inState = State::StartBit;
                        break;
                    case State::StartBit:
                        if (in[i++] == 1)
                            inState = State::DataBits;
                        else
                            inState = State::Idle;
                        break;
                    case State::DataBits:
                        if (inBitCounter++ < frameConfig.data_bits) {
                            auto bit = in[i++];
                            inParity ^= bit;
                            inBuffer.push_back(bit);
                        } else {
                            inBitCounter = 0;
                            inState = State::ParityBit;
                        }
                        break;
                    case State::ParityBit:
                        if (frameConfig.parity_type == Parity::Even && in[i++] == inParity
                         || frameConfig.parity_type == Parity::Odd && in[i++] == !inParity) {
                            inState = State::Idle;
                            inBuffer.clear();
                        } else
                            inState = State::StopBit;
                        inParity = 0;
                        break;
                    case State::StopBit:
                        if (inBitCounter++ < frameConfig.stop_bits) {
                            if (in[i++] != 1) {
                                inBitCounter = 0;
                                inState = State::Idle;
                                inBuffer.clear();
                            }
                        } else {
                            std::cout << "RX: ";
                            inBitCounter = 0;
                            inState = State::Idle;
                            std::lock_guard<std::mutex> lock(rxLock);
                            for (auto bit : inBuffer) {
                                std::cout << bit;
                                rxBuffer.push(bit);
                            }
                            std::cout << std::endl;
                            inBuffer.clear();
                        }
                        break;
                }
            }
        }
    }
}

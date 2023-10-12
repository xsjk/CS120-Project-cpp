#include <iostream>
#include <uart.h>
#include <thread>
int main() {

    UARTDevice serial;
    serial.open(9600, 5, Parity::Even, 2);
    std::cout << "Hello, World!" << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(1));
    serial.write('A');
    std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}
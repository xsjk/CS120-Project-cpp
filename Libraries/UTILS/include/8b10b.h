#pragma once

#include <unordered_map>
#include <bitset>

class B8B10
{
private:
    static std::unordered_map<std::bitset<8>, std::bitset<10>> B8_to_B10;
    static std::unordered_map<std::bitset<10>, std::bitset<8>> B10_to_B8;
public:
    static const std::bitset<10> &encode(const std::bitset<8> &b8) {
        return B8_to_B10[b8];
    }
    static const std::bitset<8> &decode(const std::bitset<10> &b10) {
        return B10_to_B8[b10];
    }
};
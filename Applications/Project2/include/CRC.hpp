#pragma once

#include <cstdint>
#include <span>
#include <array>

#include "utils.hpp"

template<std::uint8_t gen_poly>
struct CRC8 {

    static constexpr std::array<std::uint16_t, 256> prod = sorted([] {
        std::uint16_t gen = gen_poly | (1 << 8);
        std::array<std::uint16_t, 256> p{};
        for (std::uint16_t i = 0; i < 256; ++i)
            for (int j = 0; j < 8; ++j)
                if ((i >> j) & 1)
                    p[i] ^= (gen << j);
        return p;
    }());

    inline static std::uint8_t get(std::span<const std::uint8_t> data) {
        std::uint8_t org = 0;
        for (auto d : data)
            org = ((org << 8) | d) ^ prod[org];
        return (org << 8) ^ prod[org];
    }
    
    inline static bool check(std::span<const std::uint8_t> data, std::uint8_t crc) {
        std::uint16_t org = 0;
        for (auto d : data)
            org = ((org << 8) | d) ^ prod[org];
        return ((org << 8) | crc) ^ prod[org] = 0;
    }

};

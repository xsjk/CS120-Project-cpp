#pragma once

#include <cstdint>
#include <span>
#include <array>

#include "utils.hpp"

#define CRC_OK 0

template<std::uint8_t gen_poly>
struct CRC8 {

    static constexpr std::array<std::uint16_t, 256> prod = utils::sorted([] {
        std::uint16_t gen = gen_poly | (1 << 8);
        std::array<std::uint16_t, 256> p {};
        for (std::uint16_t i = 0; i < 256; ++i)
            for (int j = 0; j < 8; ++j)
                if ((i >> j) & 1)
                    p[i] ^= (gen << j);
        return p;
    }());

    inline static constexpr std::uint8_t divide(std::span<const std::uint8_t> data) {
        std::uint8_t q = 0;
        for (auto d : data)
            q = ((q << 8) | d) ^ prod[q];
        return q;
    }

    inline static constexpr std::uint8_t get(std::span<const std::uint8_t> data) {
        std::uint8_t q = divide(data);
        return (q << 8) ^ prod[q];
    }

    inline static constexpr bool check(std::span<const std::uint8_t> data, std::uint8_t crc) {
        std::uint8_t q = divide(data);
        return ((q << 8) | crc) ^ prod[q] == 0;
    }

    inline static constexpr bool check(std::span<const std::uint8_t> data) {
        return divide(data) == 0;
    }

    // iterative version
    uint8_t q = 0;
    inline void reset() { q = 0; }
    inline std::uint8_t update(std::span<const std::uint8_t> data) {
        for (auto d : data)
            q = ((q << 8) | d) ^ prod[q];
        return q;
    }
    inline std::uint8_t update(std::uint8_t data) {
        return q = ((q << 8) | data) ^ prod[q];
    }
    inline std::uint8_t get() {
        return (q << 8) ^ prod[q];
    }

};


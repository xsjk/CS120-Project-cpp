#pragma once

#include <limits>
#include <iostream>
#include <type_traits>


template<int N=14>
requires (N > 0 && N < 31)
class FixPoint {
    int data;
public:
    FixPoint() : data{} { }
    FixPoint(int v) : data(v << N) { }
    FixPoint(float v) : data(v * std::numeric_limits<int>::max()) { }
    FixPoint(const FixPoint&) noexcept = default;
    FixPoint(FixPoint&&) noexcept = default;
    FixPoint& operator=(const FixPoint&) noexcept = default;
    FixPoint& operator=(FixPoint&&) noexcept = default;
    FixPoint& operator=(int v) noexcept { return (data = v << N), *this; }
    FixPoint& operator+=(const FixPoint& v) noexcept { return (data += v.data), *this; }
    FixPoint& operator-=(const FixPoint& v) noexcept { return (data -= v.data), *this; }
    FixPoint& operator*=(const FixPoint& v) noexcept { return ((data >>= 14) *= v.data), *this; }
    FixPoint& operator/=(const FixPoint& v) noexcept { return ((data <<= 14) /= v.data), *this; }
    FixPoint& operator+=(int v) noexcept { return (data += v << N), *this; }
    FixPoint& operator-=(int v) noexcept { return (data -= v << N), *this; }
    FixPoint& operator*=(int v) noexcept { return (data *= v), *this; }
    FixPoint& operator/=(int v) noexcept { return (data /= v), *this; }
    FixPoint& operator+=(float v) noexcept { return (data += v * (1 << N)), *this; }
    FixPoint& operator-=(float v) noexcept { return (data -= v * (1 << N)), *this; }
    FixPoint& operator*=(float v) noexcept { return (data *= v), *this; }
    FixPoint& operator/=(float v) noexcept { return (data /= v), *this; }
    FixPoint operator+(const FixPoint& v) const noexcept { return FixPoint(*this) += v; }
    FixPoint operator-(const FixPoint& v) const noexcept { return FixPoint(*this) -= v; }
    FixPoint operator*(const FixPoint& v) const noexcept { return FixPoint(*this) *= v; }
    FixPoint operator/(const FixPoint& v) const noexcept { return FixPoint(*this) /= v; }

    FixPoint operator+(auto v) const noexcept { return FixPoint(*this) += v; }
    FixPoint operator-(auto v) const noexcept { return FixPoint(*this) -= v; }
    FixPoint operator*(auto v) const noexcept { return FixPoint(*this) *= v; }
    FixPoint operator/(auto v) const noexcept { return FixPoint(*this) /= v; }

    FixPoint operator-() const noexcept { return FixPoint(-data); }
    auto operator<=>(const FixPoint&) const noexcept = default;


    template<typename T>
    operator T() const noexcept { return data / T(1 << N); }

};
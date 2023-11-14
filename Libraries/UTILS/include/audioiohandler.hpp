#pragma once

#include <type_traits>

template<typename T = int, typename V = float>
    requires std::is_signed_v<T> && std::is_signed_v<V>
class ArithmeticProxy {
    T &data;

    template<typename T1, typename T2>
        requires std::is_signed_v<T1> && std::is_signed_v<T2>
    static inline constexpr T2 convert(T1 data) {
        if constexpr (std::is_integral_v<T1>) {
            if constexpr (std::is_floating_point_v<T2>)
                return (T2)data / std::numeric_limits<T1>::max();
            else if constexpr (std::is_integral_v<T2>) {
                if constexpr (sizeof(T2) < sizeof(T1))
                    return (T1)data >> (sizeof(T1) - sizeof(T2)) * CHAR_BIT;
                else if constexpr (sizeof(T2) > sizeof(T1))
                    return (T2)data << (sizeof(T2) - sizeof(T1)) * CHAR_BIT;
                else
                    return data;
            }
        } else if constexpr (std::is_floating_point_v<T1>) {
            if constexpr (std::is_floating_point_v<T2>)
                return data;
            else if constexpr (std::is_integral_v<T2>) {
                return data * std::numeric_limits<T2>::max();
            }
        }
    }

public:
    ArithmeticProxy(T &t) : data(t) { }
    ArithmeticProxy(const ArithmeticProxy<T, V> &) = delete;
    ArithmeticProxy(ArithmeticProxy<T, V> &&) = delete;
    ArithmeticProxy<T, V> &operator=(const ArithmeticProxy<T, V> &) & = delete;
    ArithmeticProxy<T, V> &operator=(ArithmeticProxy<T, V> &&) & = delete;

    operator V() const &&noexcept { 
        return convert<T, V>(data); 
    }

    auto &operator=(ArithmeticProxy<T, V> &&v) && noexcept {
        data = v.data;
        return *this;
    }

    auto &operator=(const ArithmeticProxy<T, V> &v) && noexcept {
        data = v.data;
        return *this;
    }

    auto &operator=(V v) && noexcept {
        data = convert<V, T>(v);
        return *this;
    }

    auto &operator+=(V v) && noexcept {
        data += convert<V, T>(v);
        return *this;
    }

    auto &operator-=(V v) && noexcept {
        data -= convert<V, T>(v);
        return *this;
    }

    auto &operator+=(const ArithmeticProxy<T, V> &v) && noexcept {
        data += v.data;
        return *this;
    }

    auto &operator-=(const ArithmeticProxy<T, V> &v) && noexcept {
        data -= v.data;
        return *this;
    }

    auto &operator*=(V v) && noexcept {
        data *= v;
        return *this;
    }

    auto &operator/=(V v) && noexcept {
        data /= v;
        return *this;
    }

    friend std::ostream &operator<<(std::ostream &os, const ArithmeticProxy &v) {
        return os << V(v);
    }
};


template<typename T = int, typename V = float>
    requires std::is_signed_v<T> && std::is_signed_v<T>
class AudioDataProxy {
    size_t numChannels;
    size_t numSamples;
    double sampleRate;

    class ChannelProxy {
        AudioDataProxy &dataView;
        size_t channel;
        struct iterator {
            ChannelProxy &channelView;
            size_t i;
            auto operator*() const noexcept { return channelView[i]; }
            auto &operator*() noexcept { return channelView[i]; }
            auto operator++() noexcept { i++; return *this; }
            auto operator<=>(const iterator &other) const noexcept { return i <=> other.i; }
            auto operator!=(const iterator &other) const noexcept { return i != other.i; }
            auto &operator++(int) noexcept { i++; return *this; }
            auto &operator--(int) noexcept { i--; return *this; }
            auto &operator+=(size_t n) noexcept { i += n; return *this; }
            auto &operator-=(size_t n) noexcept { i -= n; return *this; }
            auto operator+(size_t n) const noexcept { return iterator(dataView, channel, i + n); }
            auto operator-(size_t n) const noexcept { return iterator(dataView, channel, i - n); }
        };

    public:
        ChannelProxy(AudioDataProxy &dataView, size_t channel) : dataView(dataView), channel(channel) { }
        auto size() const noexcept { return dataView.getNumSamples(); }
        const auto begin() const noexcept { return iterator { const_cast<ChannelProxy &>(*this), 0 }; }
        const auto end() const noexcept { return iterator { const_cast<ChannelProxy &>(*this), dataView.size() }; }
        auto begin() noexcept { return iterator { *this, 0 }; }
        auto end() noexcept { return iterator { *this, dataView.size() }; }
        auto operator[] (size_t i) noexcept { return dataView(channel, i); }
        V operator[] (size_t i) const noexcept { return dataView(channel, i); }
    };

public:
    AudioDataProxy(size_t numChannels, size_t numSamples, size_t sampleRate) :
        numChannels(numChannels), numSamples(numSamples), sampleRate(sampleRate) { }

    virtual ArithmeticProxy<T, V> operator()(size_t i, size_t j) noexcept = 0;
    virtual V operator()(size_t i, size_t j) const noexcept = 0;
    auto operator[](size_t i) noexcept { return ChannelProxy(*this, i); }
    const auto operator[](size_t i) const noexcept { return const_cast<AudioDataProxy &>(*this)[i]; }

    virtual void zero() noexcept {
        for (auto i = 0; i < numChannels; i++)
            for (auto j = 0; j < numSamples; j++)
                (*this)(i, j) = T(0);
    }

    auto size() const noexcept { return numSamples * numChannels; }
    auto getNumChannels() const noexcept { return numChannels; }
    auto getNumSamples() const noexcept { return numSamples; }
    auto getSampleRate() const noexcept { return sampleRate; }

};



template <typename T, typename V>
struct AudioIOHandler {
    virtual void inputCallback(const AudioDataProxy<T, V> &inputData) noexcept = 0;
    virtual void outputCallback(AudioDataProxy<T, V> &outputData) noexcept = 0;
};

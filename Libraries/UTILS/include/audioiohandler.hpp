#pragma once

#include <type_traits>

template<typename T = int, typename V = float>
    requires std::is_integral_v<T>
          && std::is_signed_v<T>
          && std::is_floating_point_v<V> 
          && std::is_signed_v<V>
class FloatView {
    T &data;
public:
    FloatView(T &t) : data(t) { }
    FloatView(const FloatView<T, V>&) = delete;
    FloatView(FloatView<T, V>&&) = delete;
    FloatView<T, V>& operator=(const FloatView<T, V>&) & = delete;
    FloatView<T, V>& operator=(FloatView<T, V>&&) & = delete;

    operator V() const && noexcept {
        return (V)data / std::numeric_limits<T>::max();
    }
    
    auto& operator=(FloatView<T, V> &&v) && noexcept {
        data = v.data;
        return *this;
    }
    
    auto& operator=(const FloatView<T, V> &v) && noexcept {
        data = v.data;
        return *this;
    }

    auto &operator=(V v) && noexcept {
        data = v * std::numeric_limits<T>::max();
        return *this;
    }

    auto &operator+=(V v) && noexcept {
        data += v * std::numeric_limits<T>::max();
        return *this;
    }

    auto &operator-=(V v) && noexcept {
        data -= v * std::numeric_limits<T>::max();
        return *this;
    }

    auto &operator+=(const FloatView<T, V> &v) && noexcept {
        data += v.data;
        return *this;
    }

    auto &operator-=(const FloatView<T, V> &v) && noexcept {
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

    friend std::ostream &operator<<(std::ostream &os, const FloatView &v) {
        return os << V(v);
    }
};


template<typename T = int>
    requires std::is_integral_v<T> 
          && std::is_signed_v<T>
class AudioDataView {
    size_t numChannels;
    size_t numSamples;
    double sampleRate;

    class ChannelView {
        AudioDataView& dataView;
        size_t channel;
        struct iterator {
            ChannelView& channelView;
            size_t i;
            auto operator*() const noexcept { return channelView[i]; }
            auto& operator*() noexcept { return channelView[i]; }
            auto operator++() noexcept { i++; return *this; }
            auto operator<=>(const iterator& other) const noexcept { return i <=> other.i; }
            auto operator!=(const iterator& other) const noexcept { return i != other.i; }
            auto& operator++(int) noexcept { i++; return *this; }
            auto& operator--(int) noexcept { i--; return *this; }
            auto& operator+=(size_t n) noexcept { i += n; return *this; }
            auto& operator-=(size_t n) noexcept { i -= n; return *this; }
            auto operator+(size_t n) const noexcept { return iterator(dataView, channel, i + n); }
            auto operator-(size_t n) const noexcept { return iterator(dataView, channel, i - n); }
        };
            
    public:
        ChannelView(AudioDataView& dataView, size_t channel) : dataView(dataView), channel(channel) { }
        auto size() const noexcept { return dataView.getNumSamples(); }
        const auto begin() const noexcept { return iterator{const_cast<ChannelView&>(*this), 0}; }
        const auto end() const noexcept { return iterator{const_cast<ChannelView&>(*this), dataView.size()}; }
        auto begin() noexcept { return iterator{*this, 0}; }
        auto end() noexcept { return iterator{*this, dataView.size()}; }
        auto operator[] (size_t i) noexcept { return dataView(channel, i); }
        float operator[] (size_t i) const noexcept { return dataView(channel, i); }
    };

public:
    AudioDataView(size_t numChannels, size_t numSamples, size_t sampleRate) : 
        numChannels(numChannels), numSamples(numSamples), sampleRate(sampleRate) { }

    virtual FloatView<T> operator()(size_t i, size_t j) noexcept = 0;
    virtual float operator()(size_t i, size_t j) const noexcept = 0;
    auto operator[](size_t i) noexcept { return ChannelView(*this, i); }
    const auto operator[](size_t i) const noexcept { return const_cast<AudioDataView&>(*this)[i]; }

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



template <typename T>
struct AudioIOHandler {
    virtual void inputCallback(const AudioDataView<T> &inputData) noexcept = 0;
    virtual void outputCallback(AudioDataView<T> &outputData) noexcept = 0;
};

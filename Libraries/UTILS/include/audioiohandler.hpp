#pragma once

template<typename T = int, typename V = float>
    requires std::is_integral_v<T>
          && std::is_signed_v<T>
          && std::is_floating_point_v<V> 
          && std::is_signed_v<V>
class FloatView {
    T &data;
public:
    FloatView(T &t) : data(t) { }
    operator V() const noexcept {
        return (V)data / std::numeric_limits<T>::max();
    }
    FloatView &operator=(V v) && noexcept {
        data = v * std::numeric_limits<T>::max();
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
public:
    AudioDataView(size_t numChannels, size_t numSamples) : numChannels(numChannels), numSamples(numSamples) { }

    virtual FloatView<T> operator()(size_t i, size_t j) noexcept = 0;
    virtual float operator()(size_t i, size_t j) const noexcept = 0;

    size_t getNumChannels() const noexcept { return numChannels; }
    size_t getNumSamples() const noexcept { return numSamples; }

};



template <typename T>
struct AudioIOHandler {
    virtual void inputCallback(const AudioDataView<T> &inputData) noexcept = 0;
    virtual void outputCallback(AudioDataView<T> &outputData) noexcept = 0;
};

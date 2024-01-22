#pragma once

#include <vector>
#include <cmath>
#include <numbers>
#include <valarray>
#include <complex>
#include <algorithm>

namespace Signals {

    auto time_vector(float duration, float fs = 48000) {
        std::vector<float> t(duration * fs);
        for (auto i = 0; i < duration * fs; i++) t[i] = i / fs;
        return t;
    }

    struct Sin {
        float freq;
        float phase = 0;

        auto operator()(auto t) {
            return std::sin(2 * std::numbers::pi * freq * t + phase);
        }
    };

    struct Chirp {
        float freq_start;
        float freq_end;
        float duration;
        float phase = 0;

        float operator()(float t) {
            return std::sin(2 * std::numbers::pi * ((freq_end - freq_start) / 2 * t / duration + freq_start) * t + phase);
        }

    };

    class Hamming {
        int size;
    public:
        Hamming(int size) : size(size) { }
        float operator()(float i) {
            return 0.54 - 0.46 * std::cos(2 * std::numbers::pi * i);
        }
        float operator()(int i) {
            return (*this)(float(i) / size);
        }
        operator std::vector<float>() {
            std::vector<float> v(size);
            for (auto i = 0; i < size; i++)
                v[i] = (*this)(i);
            return v;
        }
    };


    template <typename T = float>
    struct Butter {
        std::vector<T> b, a;
        std::vector<T> x_buffer;
        std::vector<T> y_buffer;
        
        enum class Type {
            LowPass,
            HighPass,
        };

        Butter(std::vector<T> b, std::vector<T> a) : b(std::move(b)), a(std::move(a)) { }
        Butter(std::pair<std::vector<T>, std::vector<T>> coeffs) : b(std::move(coeffs.first)), a(std::move(coeffs.second)) { }
        Butter(int N, T Wn, Type btype) : Butter { [](int N, float Wn, Type btype) {
            std::vector<T> b(N + 1);
            std::vector<T> a(N + 1);
            /// TODO: implement coefficient generation
            return std::pair{ b, a };
        }(N, Wn, btype) } { }


        template<bool reset = false>
        // reset = true  :  filter without memory (used to filter a whole signal)
        // reset = false :  filter with memory (used to filter a continuous chunk of a signal)
        auto filter(auto x) {
            std::vector<T> y(x.size());

            // signal head
            if constexpr(reset) {
                for (auto i = 0; i < b.size() - 1; i++) {
                    for (auto j = 0; j < i + 1; j++)
                        y[i] += b[j] * x[i - j] - a[j] * y[i - j];
                }
            } else {
                for (auto i = 0; i < b.size() - 1; i++) {
                    for (auto j = 0; j < b.size(); j++) {
                        auto k = i - j;
                        if (k >= 0) 
                            y[i] += b[j] * x[k] - a[j] * y[k];
                        else {
                            k += x_buffer.size();
                            if (k >= 0)
                                y[i] += b[j] * x_buffer[k] - a[j] * y_buffer[k];
                            else
                                break;
                        }
                    }
                }
            }

            // signal body
            for (auto i = b.size() - 1; i < x.size(); i++)
                for (auto j = 0; j < b.size(); j++)
                    y[i] += b[j] * x[i - j] - a[j] * y[i - j];
        
            // signal tail
            if constexpr(!reset) {
                for (auto i = 0; i < b.size(); i++) {
                    auto k = x.size() - b.size() + i;
                    if (k < 0) break;
                    if (x_buffer.size() <= i) {
                        x_buffer.push_back(x[k]);
                        y_buffer.push_back(y[k]);
                    } else {
                        x_buffer[i] = x[k];
                        y_buffer[i] = y[k];
                    }
                }
            } 

            return y;
        }

        void clean() {
            x_buffer.clear();
            y_buffer.clear();
        }

        bool full() {
            return x_buffer.size() == b.size();
        }


        auto operator()(auto x) { return filter<true>(x); }

    };


    
    int log2(int n) {
        return (n <= 1) ? 0 : 1 + log2(n / 2);
    }

    template<typename T>
    void fft(std::vector<std::complex<T>>& x) {
        const int N = x.size();
        if (N <= 1) return;

        std::vector<std::complex<T>> even(N / 2), odd(N / 2);
        for (int i = 0; i < N / 2; ++i) {
            even[i] = x[2 * i];
            odd[i] = x[2 * i + 1];
        }

        fft(even);
        fft(odd);

        for (int k = 0; k < N / 2; ++k) {
            std::complex<T> t = std::polar<T>(1.0, -2 * std::numbers::pi * k / N) * odd[k];
            x[k] = even[k] + t;
            x[k + N / 2] = even[k] - t;
        }
    }

    template<typename T>
    void fft(std::valarray<std::complex<T>>& x) {
        const int N = x.size();
        if (N <= 1) return;

        std::valarray<std::complex<T>> even = x[std::slice(0, N / 2, 2)];
        std::valarray<std::complex<T>> odd = x[std::slice(1, N / 2, 2)];

        fft(even);
        fft(odd);

        for (int k = 0; k < N / 2; ++k) {
            std::complex<T> t = std::polar(1.0, -2 * std::numbers::pi * k / N) * odd[k];
            x[k] = even[k] + t;
            x[k + N / 2] = even[k] - t;
        }
    }

    template<typename T>
    void fft(std::vector<T>& x) {
        std::vector<std::complex<T>> cx(x.size());
        for (int i = 0; i < x.size(); ++i) {
            cx[i] = std::complex<T>(x[i], 0);
        }
        fft(cx);
        for (int i = 0; i < x.size(); ++i) {
            x[i] = std::abs(cx[i]);
        }
    }

    template<typename T>
    void fft(std::valarray<T>& x) {
        std::valarray<std::complex<T>> cx(x.size());
        for (int i = 0; i < x.size(); ++i) {
            cx[i] = std::complex<T>(x[i], 0);
        }
        fft(cx);
        for (int i = 0; i < x.size(); ++i) {
            x[i] = cx[i].real();
        }
    }

    template<typename T>
    void ifft(std::vector<std::complex<T>>& x) {
        std::transform(x.begin(), x.end(), x.begin(), std::conj<T>);
        fft(x);
        std::transform(x.begin(), x.end(), x.begin(), std::conj<T>);
        std::transform(x.begin(), x.end(), x.begin(), [&](std::complex<T> c) { return c / x.size(); });
    }

    template<typename T>
    void ifft(std::valarray<std::complex<T>>& x) {
        std::transform(x.begin(), x.end(), x.begin(), std::conj<T>);
        fft(x);
        std::transform(x.begin(), x.end(), x.begin(), std::conj<T>);
        std::transform(x.begin(), x.end(), x.begin(), [&](std::complex<T> c) { return c / x.size(); });
    }

    template<typename T>
    void ifft(std::vector<T>& x) {
        std::vector<std::complex<T>> cx(x.size());
        for (int i = 0; i < x.size(); ++i) {
            cx[i] = std::complex<T>(x[i], 0);
        }
        ifft(cx);
        for (int i = 0; i < x.size(); ++i) {
            x[i] = cx[i].real();
        }
    }

    template<typename T>
    void ifft(std::valarray<T>& x) {
        std::valarray<std::complex<T>> cx(x.size());
        for (int i = 0; i < x.size(); ++i) {
            cx[i] = std::complex<T>(x[i], 0);
        }
        ifft(cx);
        for (int i = 0; i < x.size(); ++i) {
            x[i] = cx[i].real();
        }
    }


    
}

#pragma once

#include <algorithm>
#include <vector>
#include <span>
#include <ranges>
#include <bitset>
#include <iostream>
#include <fstream>
#include <typeinfo>
#include <string>
#include <memory>
#include <optional>
#include <string_view>
#include <cxxabi.h>
#include <generator>

namespace utils {

    template<int start, int end>
    inline void static_for(auto &&f) {
        if constexpr (start < end) {
            f(std::integral_constant<int, start>{});
            static_for<start + 1, end>(std::forward<decltype(f)>(f));
        }
    }

    template<typename T>
    inline std::vector<T> from_file(std::string fileName) {
        std::vector<T> container;
        std::ifstream dataFile { fileName };
        T t;
        while (dataFile >> t)
            container.push_back(t);
        return container;
    }

    inline void to_file(std::string fileName, auto &&container) {
        std::ofstream dataFile { fileName };
        for (auto &&t : std::forward<decltype(container)>(container))
            dataFile << std::forward<decltype(t)>(t) << std::endl;
    }

    class BitsContainer : public std::vector<bool> {

    public:
        using value_type = bool;
        using std::vector<bool>::vector;
        using std::vector<bool>::operator=;

        auto data() const {
            #ifdef _MSC_VER
                return begin()._Myptr;
            #else
                return begin()._M_p;
            #endif
        }

        template<typename T>
        auto as_span() { return std::span((T *)data(), size() / CHAR_BIT / sizeof(T)); }

        template<typename T>
        auto as_span() const { return std::span((const T *)data(), size() / CHAR_BIT / sizeof(T)); }

        template<typename T>
        void push(T t) {
            if constexpr (std::is_same_v<std::decay_t<T>, bool>)
                push_back(t);
            else {
                for (int i = 0; i < sizeof(T) * CHAR_BIT; i++) {
                    push_back(t & 1);
                    t >>= 1;
                }
            }
        }

        template<size_t N>
        auto get(size_t i) {
            std::bitset<N> bs;
            for (int j = 0; j < N; j++)
                bs[j] = operator[](i * N + j);
            return bs;
        }

        template<size_t N>
        void push(std::bitset<N> bs) {
            for (int i = 0; i < N; i++)
                push_back(bs[i]);
        }

        static auto from_bin(std::string fileName) {
            std::ifstream dataFile { fileName, std::ios::binary };
            if (!dataFile.is_open())
                throw std::runtime_error(std::format("Cannot open file: {}", fileName));
            BitsContainer container;
            char byte;
            while (dataFile.read(&byte, sizeof(byte)))
                container.push(byte);
            return container;
        }

        void to_bin(std::string fileName) {
            std::ofstream dataFile { fileName, std::ios::binary };
            char byte;
            for (auto const &byte : as_span<char>())
                dataFile.write(&byte, sizeof(byte));
        }

        static auto from_file(std::string fileName) {
            std::ifstream dataFile { fileName };
            BitsContainer container;
            bool bit;
            while (dataFile >> bit)
                container.push(bit);
            return container;
        }

        void to_file(std::string fileName) {
            std::ofstream dataFile { fileName };
            for (const auto &bit : *this)
                dataFile << bit << std::endl;
        }

        friend std::ostream &operator<<(std::ostream &os, const BitsContainer &container) {
            for (auto bit : container)
                os << bit;
            return os;
        }

    };


    class ByteContainer : public std::vector<uint8_t> {

    public:
        using value_type = uint8_t;
        using std::vector<uint8_t>::vector;
        using std::vector<uint8_t>::operator=;

        template<typename T>
        auto as_span() { return std::span((T *)data(), size() / sizeof(T)); }

        template<typename T>
        auto as_span() const { return std::span((const T *)data(), size() / sizeof(T)); }

        void push(const auto &t) {
            for (auto c : std::span((const uint8_t *)&t, sizeof(t)))
                push_back(c);
        }

        void add_header(const auto &t) {
            auto sp = std::span((const uint8_t *)&t, sizeof(t));
            insert(begin(), sp.begin(), sp.end());
        }

        friend std::ostream &operator<<(std::ostream &os, const ByteContainer &container) {
            for (auto i = 0; i < container.size(); i++) {
                os << std::format("{:02x} ", container[i]);
                if (i % 8 == 7) 
                    os << ' '; 
                if (i % 16 == 15) 
                    os << std::endl; 
            }
            return os;
        }

    };


    class BitView {
        std::uint8_t *_data;
        std::size_t _size;

    public:
        std::_Bit_reference operator[](size_t i) {
            return std::_Bit_reference((std::_Bit_type *)&_data[i / CHAR_BIT], i % CHAR_BIT);
        }
        BitView(void *data, size_t size) : _data((std::uint8_t*)data), _size(size) { }
        auto size() const { return _size; }
        auto data() const { return _data; }
        template<size_t N>
        auto get(size_t i) {
            std::bitset<N> bs;
            for (int j = 0; j < N; j++)
                bs[j] = operator[](i * N + j);
            return bs;
        }
    };


    float mean(auto v) {
        float sum = 0;
        for (auto x : v) sum += x;
        return sum / v.size();
    }

    float mean(auto v, auto f) {
        float sum = 0;
        for (auto x : v) sum += f(x);
        return sum / v.size();
    }

    float var(auto v) {
        float m = mean(v);
        float sum = 0;
        for (auto x : v) sum += (x - m) * (x - m);
        return sum / v.size();
    }


    constexpr int count_bits(int n) {
        int count = 0;
        while (n) {
            count++;
            n >>= 1;
        }
        return count;
    }

    static constexpr auto sorted(auto arr) {
        auto ret = arr;
        std::sort(ret.begin(), ret.end());
        return ret;
    }


    template<class T1, class T2>
    class ConstMergedView {
        const T1 &left;
        const T2 &right;
    public:
        ConstMergedView(const T1 &l, const T2 &r) : left(l), right(r) { }
        auto size() { return left.size() + right.size(); }
        auto operator[](auto i) const { return i < left.size() ? left[i] : right[i - left.size()]; }
        auto begin() const {
            return iterator(left.begin(), left.end(), right.begin(), right.end());
        }

        struct iterator {
            typename T1::const_iterator it1, it1_end;
            typename T2::const_iterator it2, it2_end;

            iterator(typename T1::const_iterator it1_, typename T1::const_iterator it1_end_,
                     typename T2::const_iterator it2_, typename T2::const_iterator it2_end_)
                : it1(it1_), it1_end(it1_end_), it2(it2_), it2_end(it2_end_) { }

            auto operator*() const {
                return it1 != it1_end ? *it1 : *it2;
            }

            iterator &operator++() {
                if (it1 != it1_end) ++it1;
                else ++it2;
                return *this;
            }

            bool operator==(const iterator &other) const {
                return it1 == other.it1 && it2 == other.it2;
            }

            bool operator!=(const iterator &other) const {
                return !(*this == other);
            }
        };

    };


    template <typename T>
    std::string dynamic_typename() {
        const char *mangledName = typeid(T).name();
        int status = -1;

        // __cxa_demangle allocates memory for the demangled name using malloc
        // and returns it. We need to free this memory ourselves.
        std::unique_ptr<char, void (*)(void *)> demangledName(
            abi::__cxa_demangle(mangledName, nullptr, nullptr, &status),
            std::free
        );

        // If demangling is successful, status is set to 0
        if (status == 0 && demangledName)
            return demangledName.get();
        else
            return mangledName;
    }

    std::string dynamic_typename(auto &&t) {
        return dynamic_typename<decltype(t)>();
    }

    template <typename T>
    constexpr auto static_typename() {
        using namespace std::string_view_literals;
        #if defined(__clang__) || defined(__GNUC__)
            constexpr auto prefix = "T = "sv;
            constexpr auto suffix = "]"sv;
            constexpr std::string_view function = __PRETTY_FUNCTION__;
        #elif defined(_MSC_VER)
            constexpr auto prefix = "get_type_name<"sv;
            constexpr auto suffix = ">(void)"sv;
            constexpr std::string_view function = __FUNCSIG__;
        #else
            #error Unsupported compiler
        #endif
        constexpr auto start = function.find(prefix) + prefix.size();
        return function.substr(start, function.rfind(suffix) - start);
    }

    std::string_view static_typename(auto &&t) {
        return static_typename<decltype(t)>();
    }


    template<std::ranges::input_range R>
    std::generator<typename R::value_type> concat(R &&r) {
        for (auto &&e : r)
            co_yield e;
    }

    template<std::ranges::input_range R, std::ranges::input_range... Rs>
    std::generator<typename R::value_type> concat(R &&r, Rs &&...rs) {
        for (auto &&e : r)
            co_yield e;
        for (auto &&e : concat(std::forward<Rs>(rs)...))
            co_yield e;
    }


} // namespace utils


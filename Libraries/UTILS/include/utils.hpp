#pragma once

#include <algorithm>
#include <vector>
#include <span>
#include <ranges>
#include <bitset>
#include <iostream>
#include <typeinfo>
#include <string>
#include <memory>
#include <string_view>

template<int start, int end>
inline void static_for(auto&& f) {
    if constexpr (start < end) {
        f(std::integral_constant<int, start>{});
        static_for<start + 1, end>(std::forward<decltype(f)>(f));
    }
}


class BitsContainer : public std::vector<bool> {

public:
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
    auto as_span() { return std::span((T*)data(), size() / CHAR_BIT / sizeof(T)); }

    template<typename T>
    auto as_span() const { return std::span((const T*)data(), size() / CHAR_BIT / sizeof(T)); }

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
        for (int j = 0; j < N; j++) {
            bs[j] = operator[](i * N + j);
        }
        return bs;
    }

    template<size_t N>
    void push(std::bitset<N> bs) {
        for (int i = 0; i < N; i++) {
            push_back(bs[i]);
        }
    }

};


class ByteContainer : public std::vector<uint8_t> {

public:
    using std::vector<uint8_t>::vector;
    using std::vector<uint8_t>::operator=;

    template<typename T>
    auto as_span() { return std::span((T*)data(), size() / sizeof(T)); }

    template<typename T>
    auto as_span() const { return std::span((const T*)data(), size() / sizeof(T)); }

    void push(const auto& t) {
        for (auto c: std::span((const uint8_t*)&t, sizeof(t)))
            push_back(c);
    }

    void add_header(const auto& t) {
        auto sp = std::span((const uint8_t*)&t, sizeof(t));
        insert(begin(), sp.begin(), sp.end());
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
    const T1& left;
    const T2& right;
public:
    ConstMergedView(const T1 &l, const T2& r) : left(l), right(r) {}
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
            : it1(it1_), it1_end(it1_end_), it2(it2_), it2_end(it2_end_) {}

        auto operator*() const {
            return it1 != it1_end ? *it1 : *it2;
        }

        iterator& operator++() {
            if (it1 != it1_end) ++it1;
            else ++it2;
            return *this;
        }

        bool operator==(const iterator& other) const {
            return it1 == other.it1 && it2 == other.it2;
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
    };

};


// template <typename T>
// std::string pretty_type_name() {
//     const char* mangledName = typeid(T).name();
//     int status = -1;

//     // __cxa_demangle allocates memory for the demangled name using malloc
//     // and returns it. We need to free this memory ourselves.
//     std::unique_ptr<char, void (*)(void*)> demangledName(
//         abi::__cxa_demangle(mangledName, nullptr, nullptr, &status),
//         std::free
//     );

//     // If demangling is successful, status is set to 0
//     if (status == 0 && demangledName) {
//         return demangledName.get();
//     } else {
//         return mangledName;
//     }
// }


template <typename T>
constexpr auto pretty_type_name(const T&) {
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

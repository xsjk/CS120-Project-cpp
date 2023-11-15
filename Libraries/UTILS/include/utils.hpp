#pragma once

#include <algorithm>

template<int start, int end>
inline void static_for(auto&& f) {
    if constexpr (start < end) {
        f(std::integral_constant<int, start>{});
        static_for<start + 1, end>(std::forward<decltype(f)>(f));
    }
}


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


#ifndef SAMPLE_FRAME_HPP
#define SAMPLE_FRAME_HPP

#include <utility>
#include <type_traits>

using AssignPair = std::pair<std::size_t, std::size_t>;

template<typename T, std::size_t N, typename = std::enable_if_t<!std::is_reference_v<T>>>
struct SampleFrame {
    using valueType = T;
    static constexpr std::size_t channel = N;

    T data[N];
    constexpr SampleFrame() = default;
    template<typename... Args>
    constexpr SampleFrame(Args... args) : data{static_cast<T>(std::forward<Args>(args))...} {}
    constexpr T& operator[](std::size_t i) {return data[i];}
    constexpr const T& operator[](std::size_t i) const {
        return data[i];
    }
};

template<typename T, std::size_t N_left, std::size_t N_right, typename First, typename... Others>
inline SampleFrame<T, N_left> to(SampleFrame<T, N_left>& lhs, const SampleFrame<T, N_right>& rhs, First head, Others&&... others) {
    lhs[head.first] = rhs[head.second];
    if constexpr (sizeof...(Others) != 0) {
        to(lhs, rhs, std::forward<Others>(others)...);
    }
    return lhs;
}

#endif //SAMPLE_FRAME_HPP
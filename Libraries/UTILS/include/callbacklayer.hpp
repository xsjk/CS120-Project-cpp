#include <vector>
#include <type_traits>
#include <vector>
#include <iostream>


namespace OSI {

// from the lowest to the highest, trigger callback event level by level
template <class T, class... Ts>
struct MultiLayerHandler {
    using head = T;
    using tail = MultiLayerHandler<Ts...>;

    head handler;
    tail upper;

    void inputCallback(auto&& input) {
        // calculate the upper_input;
        // input -> upper_input
        // then trigger the upper layer inputCallback
        if constexpr (std::is_void_v<decltype(handler.passUpper(std::move(input)))>)
            handler.passUpper(std::move(input));
        else {
            auto upper_input = handler.passUpper(std::move(input));
            if (upper_input.size() > 0)
                upper.inputCallback(std::move(upper_input));
        }
    }

    auto outputCallback(auto& output) {
        handler.passLower(upper.outputCallback(), output);
    }

    auto outputCallback() {
        return handler.passLower(upper.outputCallback());
    }

};


// top layer
template <typename T>
struct MultiLayerHandler<T> {
    using head = T;
    using tail = void;
    using LowerData = T::LowerData;

    head handler;

    void inputCallback(auto&& input) {
        handler.passUpper(std::move(input));
    }
    auto outputCallback() {
        return handler.passLower();
    }
};


template<class ...Ts>
class MultiLayerIOHandler : public IOHandler<float> {
public:

    OSI::MultiLayerHandler<Ts...> handler;

    void outputCallback(DataView<float> &p) noexcept override {
        handler.outputCallback(p);
    }

    void inputCallback(DataView<float> &&p) noexcept override {
        handler.inputCallback(std::move(p));
    }

};


} // namespace OSI





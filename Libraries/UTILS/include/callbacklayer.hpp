#include <vector>
#include <type_traits>
#include <vector>
#include <iostream>


namespace OSI {

template<class T>
concept isTopLayerHandler = requires(T a) {
    { a.inputCallback(std::declval<const typename T::LowerData&>()) } -> std::same_as<void>;
    { a.outputCallback(std::declval<typename T::LowerData&>()) } -> std::same_as<void>;
};


template<class T>
concept isLayerHandler = requires(T a) {
    { a.passUpper(std::declval<const typename T::LowerData&>(), std::declval<typename T::UpperData&>()) } -> std::same_as<void>;
    { a.passLower(std::declval<const typename T::UpperData&>(), std::declval<typename T::LowerData&>()) } -> std::same_as<void>;
};


// from the lowest to the highest, trigger callback event level by level
template <class T, class... Ts>
    requires isLayerHandler<T> || (sizeof...(Ts) == 0 && isTopLayerHandler<T>)
struct MultiLayerHandler {
    using head = T;
    using tail = MultiLayerHandler<Ts...>;
    using LowerData = T::LowerData;
    using UpperData = T::UpperData;

    head handler;
    tail upper;

    void inputCallback(const LowerData& input) {
        // may write to the input buffer
        UpperData upper_input;
        // calculate the upper_input;
        // input -> upper_input
        handler.passUpper(input, upper_input);
        if (upper_input.size() > 0)
            upper.inputCallback(upper_input);
        // do not invoke upper->inputCallback if upper_input is empty
    }
    void outputCallback(LowerData& output) {
        UpperData upper_output;
        upper.outputCallback(upper_output);
        if (upper_output.size() > 0) {
            // calculate the output of this layer based on the ouput of upper layer
            // upper_output -> ouput
            handler.passLower(upper_output, output);
        }
    }

};


// top layer
template <typename T>
    requires isTopLayerHandler<T>
struct MultiLayerHandler<T> {
    using head = T;
    using tail = void;
    using LowerData = T::LowerData;

    head handler;

    void inputCallback(const LowerData& input) {
        handler.inputCallback(input);
    }
    void outputCallback(LowerData& output) {
        handler.outputCallback(output);
    }
};


template<class ...Ts>
class MultiLayerIOHandler : public IOHandler<float> {
public:

    OSI::MultiLayerHandler<Ts...> handler;

    void outputCallback(DataView<float> &p) noexcept override {
        handler.outputCallback(p);
    }

    void inputCallback(const DataView<float> &p) noexcept override {
        handler.inputCallback(p);
    }

};


} // namespace OSI





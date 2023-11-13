#pragma once

#include <functional>
#include <queue>
#include <mutex>

namespace Physical {

template<typename T>
class Generator {
    std::function<T(int)> func;
    int ticks;
    int current_tick = 0;
public:
    std::string name;
    Generator(std::function<T(int)> func, int ticks, std::string name = "")
        : func(std::move(func)), ticks(ticks), name(std::move(name)) { }

    const Generator& operator=(const Generator&) = delete;

    T next() { 
        // if (current_tick == 0) 
        //     std::cout << "(Sender) " << name << " started " << '\n';
        return func(current_tick++); 
    }
    bool empty() { return current_tick >= ticks; }
    int size() { return ticks - current_tick; }
    T operator()() { return next(); }
    operator bool() { return !empty(); }

};

template<typename T>
class GeneratorQueue {
    std::queue<Generator<T>> queue;
    std::mutex mutex;
    int ticks = 0;
public:

    void push(Generator<T> gen) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(std::move(gen));
        ticks += queue.front().size();
    }

    Generator<T> pop() {
        std::lock_guard<std::mutex> lock(mutex);
        auto gen = queue.front();
        queue.pop();
        ticks -= gen.size();
        return gen;
    }

    bool empty() {
        return queue.empty();
    }

    int size() {
        return ticks;
    }

    T next() {
        if (queue.empty())
            return 0;
        auto& gen = queue.front();
        if (gen.empty()) {
            pop();
            return next();
        }
        ticks -= 1;
        return gen.next();
    }

    T operator()() { return next(); }
};

}

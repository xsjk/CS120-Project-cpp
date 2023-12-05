#include "asyncio.hpp"
async def task1() -> awaitable<int> {
    for (int i = 0; i < 5; ++i) {
        std::printf("task1: %d\n", i);
        co_await asyncio.sleep(1s);
    }
    co_return 0;
}

async def task2() -> awaitable<int> {
    for (int i = 0; i < 5; ++i) {
        std::printf("task2: %d\n", i);
        co_await asyncio.sleep(1s);
    }
    co_return 1;
}

int main() {

    std::cout << "asyncio.gather(...) test" << std::endl;
    asyncio.run([&] () -> awaitable<void> {
        auto [i, j] = co_await asyncio.gather(task1(), task2());
        std::cout << i << ' ' << j << std::endl;
        co_return;
    }());

    std::cout << "asyncio.gather(std::vector) test" << std::endl;
    asyncio.run([&] () -> awaitable<void> {
        std::vector<awaitable<int>> tasks;
        tasks.push_back(task1());
        tasks.push_back(task2());
        auto r = co_await asyncio.gather(std::move(tasks));
        std::cout << r[0] << ' ' << r[1] << std::endl;
        co_return;
    }());

    std::cout << "asyncio.wait_for(...) test" << std::endl;
    asyncio.run([&] () -> awaitable<void> {
        try {
            auto r = co_await asyncio.wait_for(task1(), 2s);
            std::cout << r << std::endl;
        } catch (const TimeoutError& e) {
            std::cerr << e.what() << std::endl;
        }
        co_return;
    }());

    return 0;
}


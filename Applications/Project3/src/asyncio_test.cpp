#include "asyncio.hpp"
async def task1() -> awaitable<int> {
    for (int i = 0; i < 10; ++i) {
        std::printf("task1: %d\n", i);
        co_await asyncio.sleep(1s);
    }
    co_return 0;
}

async def task2() -> awaitable<int> {
    for (int i = 0; i < 10; ++i) {
        std::printf("task2: %d\n", i);
        co_await asyncio.sleep(1s);
    }
    co_return 0;
}

int main() {

    asyncio.run([&] () -> awaitable<void> {
        co_await asyncio.gather(task1(), task2());
        co_return;
    }());

    return 0;
}


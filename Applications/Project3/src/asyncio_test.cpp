#include "asyncio.hpp"
async def task1() -> awaitable<int> {
    std::cout << "T1 " << std::this_thread::get_id() << std::endl;
    for (int i = 0; i < 5; ++i) {
        std::printf("task1: %d\n", i);
        co_await asyncio.sleep(1s);
    }
    co_return 0;
}

async def task2() -> awaitable<int> {
    std::cout << "T2 " << std::this_thread::get_id() << std::endl;
    for (int i = 0; i < 5; ++i) {
        std::printf("task2: %d\n", i);
        co_await asyncio.sleep(1s);
    }
    co_return 1;
}

async def task3() -> awaitable<void> {
    for (int i = 0; i < 5; ++i) {
        std::printf("task3: %d\n", i);
        co_await asyncio.sleep(1s);
    }
    co_return;
}

def task4(int k, int j) {
    for (int i = 0; i < j; ++i) {
        std::printf("task%d: %d\n", k, i);
        std::this_thread::sleep_for(1s);
    }
    return k;
}

int main() {

    std::cout << "M " << std::this_thread::get_id() << std::endl;

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
            co_await asyncio.wait_for(task3(), 2s);
        } catch (const TimeoutError& e) {
            std::cerr << e.what() << std::endl;
        }
        co_return;
    }());

    std::cout << "asyncio.to_thread(...) test" << std::endl;

    asyncio.run([&] () -> awaitable<void> {
        co_await asyncio.gather(
            task3(), 
            asyncio.to_thread(std::function(task4), 4, 2),
            asyncio.to_thread(std::function(task4), 4, 3)
        );
        co_return;
    }());

    return 0;
}


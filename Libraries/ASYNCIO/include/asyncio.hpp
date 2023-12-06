#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/promise.hpp>
#include <iostream>
#include <optional>
#include <ranges>
#include <thread>
#include "utils.hpp"

#define async
#define def auto

using boost::asio::awaitable;
using namespace std::chrono_literals;
using namespace boost::asio::experimental::awaitable_operators;

template <typename T>
struct is_awaitable_t : std::bool_constant<false> { };

template <typename T>
struct is_awaitable_t<boost::asio::awaitable<T>> : std::bool_constant<true> { };

template <typename T>
constexpr bool is_awaitable_v = is_awaitable_t<T>::value;

template <typename T>
concept Awaitable = is_awaitable_v<T>;



struct TimeoutError : std::runtime_error {
    TimeoutError(auto time) : std::runtime_error(
        std::format("Timeout after {}", time)) { }
};

class Asyncio {
    boost::asio::io_context ctx;
    boost::asio::thread_pool pool;
    boost::asio::signal_set signals;
public:
    Asyncio() : signals(ctx, SIGINT, SIGTERM) {
        signals.async_wait([&](const boost::system::error_code &error, int signal) {
            if (error) {
                std::cout << "Error: " << error.message() << std::endl;
            }
            else if (signal == SIGINT) {
                std::cout << "SIGINT" << std::endl;
            }
            else if (signal == SIGTERM) {
                std::cout << "SIGTERM" << std::endl;
            }
            close();
        });
    }

    template<Awaitable T>
    auto create_task(T &&coro) {
        using boost::asio::experimental::awaitable_operators::detail::awaitable_wrap;
        return boost::asio::co_spawn(ctx, awaitable_wrap(std::move(coro)), boost::asio::deferred);
    }

    auto create_task(awaitable<void> &&coro) {
        return boost::asio::co_spawn(ctx, std::move(coro), boost::asio::deferred);
    }

    template<Awaitable T>
    auto detach_task(T &&coro) {
        return boost::asio::co_spawn(ctx, std::move(coro), boost::asio::detached);
    }

    template<Awaitable T>
    void run(T &&coro) {
        ctx.restart();
        detach_task([&](auto &&coro) -> awaitable<void> {
            try {
                co_await std::move(coro);
            }
            catch (const std::exception &e) {
                std::cout << "Exception: " << e.what() << std::endl;
            }
            close();
        }(std::move(coro)));
        mainloop();
    }

    template<typename V>
    awaitable<V> wait_for(awaitable<V> &&coro, auto time_out) {
        try {
            co_return std::get<0>(co_await(std::move(coro) || sleep(time_out)));
        }
        catch (const std::bad_variant_access &e) {
            throw TimeoutError(time_out);
        }
    }

    awaitable<void> wait_for(awaitable<void> &&coro, auto time_out) {
        try {
            std::get<0>(co_await(std::move(coro) || sleep(time_out)));
        }
        catch (const std::bad_variant_access &e) {
            throw TimeoutError(time_out);
        }
    }


    template<typename F, typename ...Args, typename R = std::invoke_result_t<F, Args...>>
    boost::asio::awaitable<std::invoke_result_t<F, Args...>> to_thread(F &&f, Args&&... args) {
        std::promise<R> promise;

        boost::asio::post(pool, [
            &p = promise,
            f = std::forward<F>(f),
            args = std::make_tuple(std::forward<Args>(args)...)
        ]() mutable {
            try {
                if constexpr (std::is_void_v<R>) {
                    std::apply(f, std::move(args));
                    p.set_value();
                }
                else {
                    p.set_value(std::apply(f, std::move(args)));
                }
            }
            catch (...) {
                p.set_exception(std::current_exception());
            }
        });

        auto future = promise.get_future();
        while (future.wait_for(0s) != std::future_status::ready)
            co_await sleep(0s);

        if constexpr (std::is_void_v<R>)
            co_return;
        else
            co_return future.get();
    }

    template<Awaitable... T>
    static inline auto gather(T&&... corr) { 
        return (... && std::forward<T>(corr)); 
    }

    template<Awaitable... T>
    static inline auto gather(std::tuple<T...> &&corr) {
        return std::apply([](auto &&... corr) { return (gather(std::move(corr)...)); }, std::move(corr));
    }

    template<typename V>
        requires (!std::is_void_v<V>)
    awaitable<std::vector<V>> gather(std::vector<awaitable<V>> &&coros) {
        using namespace boost::asio::experimental;
        auto [order, errors, results] = co_await make_parallel_group([&]() {
            using Task = decltype(create_task(std::declval<awaitable<V>>()));
            std::vector<Task> tasks;
            std::ranges::transform(coros, std::back_inserter(tasks), [&](auto &&coro) {
                return create_task(std::move(coro));
            });
            return tasks;
        }()).async_wait(
            wait_for_all(),
            boost::asio::deferred
        );
        co_return std::move(results);
    }

    awaitable<void> gather(std::vector<awaitable<void>> &&coros) {
        using namespace boost::asio::experimental;
        auto [order, errors] = co_await make_parallel_group([&]() {
            using Task = decltype(create_task(std::declval<awaitable<void>>()));
            std::vector<Task> tasks;
            std::ranges::transform(coros, std::back_inserter(tasks), [&](auto &&coro) {
                return create_task(std::move(coro));
            });
            return tasks;
        }()).async_wait(
            wait_for_all(),
            boost::asio::deferred
        );
        co_return;
    }

    boost::asio::awaitable<void> sleep(auto time) {
        co_await boost::asio::steady_timer(ctx, time).async_wait(boost::asio::use_awaitable);
    }


private:

    void close() {
        ctx.stop();
        pool.stop();
    }

    void mainloop() {
        ctx.run();
        pool.join();
    }

} asyncio;



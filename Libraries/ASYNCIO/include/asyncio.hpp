#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <iostream>
#include <optional>
#include <ranges>
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
        std::format("Timeout after {}", time)) {}
};

class Asyncio {
    boost::asio::io_context ctx;
    boost::asio::signal_set signals;
public:
    Asyncio() : signals(ctx, SIGINT, SIGTERM) {
        signals.async_wait([&](const boost::system::error_code &error, int signal) {
            if (error) {
                std::cout << "Error: " << error.message() << std::endl;
            } else if (signal == SIGINT) {
                std::cout << "SIGINT" << std::endl;
            } else if (signal == SIGTERM) {
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
        std::variant<std::monostate, int> res = co_await (
            sleep(time_out) || std::move(coro)
        );
        try {
            co_return std::get<V>(res);
        } catch (const std::bad_variant_access& e) {
            throw TimeoutError(time_out);
        }
    }

    template<Awaitable... T> 
    static inline auto gather(T&&... corr) { return (std::move(corr) && ...); }
    
    template<Awaitable... T>
    static inline auto gather(std::tuple<T...> &&corr) {
        return std::apply([](auto &&... corr) { return (gather(std::move(corr)...)); }, std::move(corr));
    }

    template<typename V>
    requires (!std::is_void_v<V>)
    awaitable<std::vector<V>> gather(std::vector<awaitable<V>> &&coros) {
        using namespace boost::asio;
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
            deferred
        );
        co_return std::move(results);
    }

    boost::asio::awaitable<void> sleep(auto time) {
        co_await boost::asio::steady_timer(ctx, time).async_wait(boost::asio::use_awaitable);
    }


private:

    void close() { ctx.stop(); }

    void mainloop() { ctx.run(); }

} asyncio;



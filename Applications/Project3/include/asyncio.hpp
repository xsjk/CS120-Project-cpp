#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <iostream>
#include <optional>
#include <ranges>
#include <utils.hpp>

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

    template<Awaitable T>
    auto wait_for(T &&coro, auto time) {
        return boost::asio::co_spawn(ctx, std::move(coro), boost::asio::use_awaitable) && sleep(time);
    }

    template<Awaitable... T> 
    static inline auto gather(T&&... corr) { return (std::move(corr) && ...); }
    
    template<Awaitable... T>
    static inline auto gather(std::tuple<T...> &&corr) {
        return std::apply([](auto &&... corr) { return (gather(std::move(corr)...)); }, std::move(corr));
    }

    boost::asio::awaitable<void> sleep(auto time) {
        co_await boost::asio::steady_timer(ctx, time).async_wait(boost::asio::use_awaitable);
    }


private:

    void close() { ctx.stop(); }

    void mainloop() { ctx.run(); }

} asyncio;



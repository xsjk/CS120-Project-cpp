#pragma once

#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <iostream>
#include <optional>
#include <ranges>
#include <thread>

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

#define def auto
#define async Awaitable

struct TimeoutError : std::runtime_error {
    TimeoutError(auto time) : std::runtime_error(
        std::format("Timeout after {}", time)) { }
};

struct CancelledError : std::runtime_error {
    CancelledError() : std::runtime_error("Cancelled") { }
};

class Context : public boost::asio::io_context {
    std::jthread thread;
    boost::asio::io_context::work work;
public:
    Context() : work(*this), thread([&] { run(); }) {}
};


class Asyncio {
    boost::asio::io_context ctx;
    boost::asio::signal_set signals;
public:
    Asyncio() : signals(ctx, SIGINT, SIGTERM) {}

    template<typename T>
    def create_task(awaitable<T> &&coro) {
        using boost::asio::experimental::awaitable_operators::detail::awaitable_wrap;
        return boost::asio::co_spawn(ctx, awaitable_wrap(std::move(coro)), boost::asio::deferred);
    }

    def create_task(awaitable<void> &&coro) {
        return boost::asio::co_spawn(ctx, std::move(coro), boost::asio::deferred);
    }

    /**
     * @brief run a async function in the background
     * 
     * @param coro the awaitable object
     */
    template<typename T>
    def detach_task(awaitable<T> &&coro) -> void {
        boost::asio::co_spawn(ctx, std::move(coro), boost::asio::detached);
    }
    
    /**
     * @brief run a sync function in the background
     *        by creating a task and detaching it
     * 
     * @param f the function to run
     * @note thread safety is not guaranteed 
     */
    def detach_task(auto &&f) -> void {
        using F = decltype(f);
        std::jthread(std::forward<F>(f)).detach();
    }

    /**
     * @brief run a async function in a sync context
     * 
     * @param coro the awaitable object
     */
    template<typename T>
    T run(awaitable<T> &&coro) {
        signals.async_wait([&](const boost::system::error_code &error, int signal) {
            if (error) {
                std::cerr << "Error: " << error.message() << std::endl;
            }
            else if (signal == SIGINT) {
                std::cerr << "SIGINT" << std::endl;
            }
            else if (signal == SIGTERM) {
                std::cerr << "SIGTERM" << std::endl;
            }
            pause();
        });
        
        resume();

        std::unique_ptr<T> result;

        detach_task([&](async def &&coro) -> awaitable<void> {
            result = std::make_unique<T>(co_await std::move(coro));
            pause();
            co_return;
        }(std::move(coro)));
        mainloop();

        return std::move(*result);
    }

    /**
     * @brief run a async function in a sync context
     * 
     * @param coro the awaitable object
     */
    void run(awaitable<void> &&coro) {
        signals.async_wait([&](const boost::system::error_code &error, int signal) {
            if (error) {
                std::cerr << "Error: " << error.message() << std::endl;
            }
            else if (signal == SIGINT) {
                std::cerr << "SIGINT" << std::endl;
            }
            else if (signal == SIGTERM) {
                std::cerr << "SIGTERM" << std::endl;
            }
            pause();
        });
        
        resume();
        detach_task([&](async def &&coro) -> awaitable<void> {
            co_await std::move(coro);
            pause();
            co_return;
        }(std::move(coro)));
        mainloop();
    }

    /**
     * @brief Wait for the single Future or coroutine to complete, with timeout.
     * 
     * @tparam V 
     * @param coro the coroutine to wait for
     * @param timeout the timeout
     * @return the result of the coroutine
     * @throw TimeoutError if the coroutine did not complete before the timeout
     */
    template<typename V>
    async def wait_for(awaitable<V> &&coro, auto timeout) -> awaitable<V> {
        try {
            co_return std::get<0>(co_await(std::move(coro) || sleep(timeout)));
        }
        catch (const std::bad_variant_access &e) {
            throw TimeoutError(timeout);
        }
    }

    /**
     * @brief Wait for the single Future or coroutine to complete, with timeout.
     * 
     * @param coro the coroutine to wait for
     * @param timeout the timeout
     * @throw TimeoutError if the coroutine did not complete before the timeout
     */
    async def wait_for(awaitable<void> &&coro, auto timeout) -> awaitable<void> {
        try {
            std::get<0>(co_await(std::move(coro) || sleep(timeout)));
        }
        catch (const std::bad_variant_access &e) {
            throw TimeoutError(timeout);
        }
    }
    
    /**
     * @brief Asynchronously run a sync function in a separate thread.
     * 
     * @param f the function to run
     * @param args... the arguments to pass to @c f
     * @return a coroutine that can be awaited to get the eventual result of the function
     */
    template<typename F, typename ...Args, typename R = std::invoke_result_t<F, Args...>>
        requires(!std::is_void_v<R>)
    async def to_thread(F &&f, Args &&...args) -> awaitable<R> {
        boost::asio::completion_token_for<void(R)> auto token = boost::asio::use_awaitable;
        using CompletionToken = decltype(token);
        return boost::asio::async_initiate<CompletionToken, void(R)>(
            [&] (
                boost::asio::completion_handler_for<void(R)> auto handler,
                auto &&f,
                std::tuple<Args...> &&args
            ) {
                auto work = boost::asio::make_work_guard(handler);
                auto executor = work.get_executor();
                detach_task([
                    executor = std::move(executor),
                    handler = std::move(handler),
                    f = std::move(f),
                    args = std::move(args)
                ]() mutable {
                    auto alloc = boost::asio::get_associated_allocator(
                        handler, boost::asio::recycling_allocator<void>());
                    boost::asio::dispatch(
                        std::move(executor),
                        boost::asio::bind_allocator(
                            alloc, 
                            std::bind(
                                std::move(handler), 
                                std::apply(std::move(f), std::move(args))
                            )
                        )
                    );
                });
            }, token,
            std::function<R(Args...)>(std::forward<F>(f)),
            std::make_tuple(std::forward<decltype(args)>(args)...)
        );
    }


    /**
     * @brief Asynchronously run a sync function in a separate thread.
     * 
     * @param f the function to run
     * @param args... the arguments to pass to @c f
     */
    template<typename F, typename ...Args>
        requires(std::is_void_v<std::invoke_result_t<F, Args...>>)
    async def to_thread(F &&f, Args &&...args) -> awaitable<void>  {
        boost::asio::completion_token_for<void()> auto token = boost::asio::use_awaitable;
        using CompletionToken = decltype(token);
        return boost::asio::async_initiate<CompletionToken, void()>(
            [&] (
                boost::asio::completion_handler_for<void()> auto handler,
                auto &&f,
                std::tuple<Args...> &&args
            ) {
                auto work = boost::asio::make_work_guard(handler);
                auto executor = work.get_executor();

                detach_task([
                    executor = std::move(executor),
                    handler = std::move(handler),
                    f = std::move(f),
                    args = std::move(args)
                ]() mutable {
                    auto alloc = boost::asio::get_associated_allocator(
                        handler, boost::asio::recycling_allocator<void>());
                    std::apply(std::move(f), std::move(args));
                    boost::asio::dispatch(
                        std::move(executor),
                        boost::asio::bind_allocator(alloc, std::move(handler))
                    );
                });
            }, token,
            std::function<void(Args...)>(std::forward<F>(f)),
            std::make_tuple(std::forward<decltype(args)>(args)...)
        );
    }


    /**
     * @brief Run all coroutines in parallel and wait until all of them are finished.
     * 
     * @param corr... the coroutines/futures to aggregate
     * @return a coroutine wrapping the results in a tuple
     */
    template<async... T>
    static async def gather(T&&... corr) { 
        return (... && std::forward<T>(corr)); 
    }

    /**
     * @brief Run all coroutines in parallel and wait until all of them are finished.
     * 
     * @param corrs a tuple of coroutines
     * @return a coroutine wrapping the results in a tuple
     */
    template<async... T>
    static async def gather(std::tuple<T...> &&corrs) {
        return std::apply([](auto &&... corr) { return (gather(std::move(corr)...)); }, std::move(corrs));
    }

    /**
     * @brief Run all coroutines in parallel and wait until all of them are finished.
     * 
     * @param corrs a vector of coroutines
     * @return a coroutine wrapping the results in a vector
     */
    template<typename V> requires (!std::is_void_v<V>)
    async def gather(std::vector<awaitable<V>> &&coros) -> awaitable<std::vector<V>> {
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

    /**
     * @brief Run all coroutines in parallel and wait until all of them are finished.
     * 
     * @param coros the container of coroutines to wait for
     * @return a coroutine wrapping void
     */
    async def gather(std::vector<awaitable<void>> &&coros) -> awaitable<void> {
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

    /**
     * @brief Coroutine that completes after a given time.
     * 
     * @param time duration to sleep (use std::chrono::duration)
     */
    static async def sleep(auto time) -> awaitable<void> {
        auto executor = co_await boost::asio::this_coro::executor;
        co_await boost::asio::steady_timer(executor, time).async_wait(boost::asio::use_awaitable);
    }

    def pause() -> void {
        ctx.stop();
    }
    
    def paused() -> bool {
        return ctx.stopped();
    }

    def resume() -> void {
        ctx.restart();
    }

    def mainloop() -> void {
        ctx.run();
    }

    def &get_context() {
        return ctx;
    }

} asyncio;



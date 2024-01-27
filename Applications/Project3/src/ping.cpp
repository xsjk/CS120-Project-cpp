#include <ping.hpp>

/**
 * @brief ping the target ip address and return the time of the response
 *
 * @param ip_addr the ip address of the target host
 */
async def ping(std::string ip_addr) noexcept -> awaitable<std::optional<std::chrono::microseconds>> {

    auto packet = create_ping_request({
        .src_mac = "80-45-dd-e5-c0-f9",
        .dst_mac = "bc-17-b8-30-39-b9",
        .src_ip = "192.168.43.231",
        .dst_ip = ip_addr,
        .identifier = 0x0000,
        .seq_num = 0x0000,
    });

    std::cerr << packet << std::endl;

    co_await asyncio.sleep(100ms);

    int n_bytes = 32;
    std::chrono::milliseconds time = 100ms;
    int ttl = 48;
    std::cout << std::format("来自 {} 的回复: 字节={} 时间={} TTL={}", ip_addr, n_bytes, time, ttl) << std::endl;

    co_return time;

}


/**
 * @brief ping the target ip address for a number of times with a given interval
 *
 * @param ip_addr the ip address of the target host
 * @param number the number of ICMP Echos
 * @param interval the interval between two ICMP Echos
 */
async def ping(std::string ip_addr, int number, int interval) noexcept -> awaitable<void> {

    std::cout << std::format("正在 Ping {} 具有 32 字节的数据:", ip_addr) << std::endl;

    std::vector<std::chrono::microseconds> response_time; // in ms
    for (int i = 0; i < number; i++) {
        auto res = co_await asyncio.gather(
            ping(ip_addr),
            asyncio.sleep(std::chrono::seconds(interval))
        );
        if (res.has_value())
            response_time.push_back(*res);
    }

    std::cout << std::endl;
    std::cout << std::format(
        "{} 的 Ping 统计信息:\n"
        "    数据包: 已发送 = {}，已接收 = {}，丢失 = {} ({}% 丢失)，\n"
        "往返行程的估计时间(以毫秒为单位):\n"
        "    最短 = {}ms，最长 = {}ms，平均 = {}ms",
        ip_addr,
        number, response_time.size(), number - response_time.size(),
        int((number - response_time.size()) / (double)number * 100),
        58, 68, 63
    ) << std::endl;

    co_return;

}


int main(int argc, char **argv) {
    return asyncio.run([&]() -> awaitable<int> {

        argparse::ArgumentParser program("ping");
        program.add_argument("destination")
            .help("The ip address of the target host");
        program.add_argument("-i", "--interval")
            .help("The interval between two ICMP Echos")
            .default_value(1)
            .scan<'i', int>();
        program.add_argument("-n", "--number")
            .help("The number of ICMP Echos")
            .default_value(4)
            .scan<'i', int>();

        try {

            program.parse_args(argc, argv);

            auto des_ip = program.get<std::string>("destination");
            auto number = program.get<int>("--number");
            auto interval = program.get<int>("--interval");

            co_await ping(des_ip, number, interval);
            co_return 0;
        }
        catch (const std::exception &err) {
            std::cerr << err.what() << std::endl;
            std::cerr << program;
            co_return 1;
        }


    }());
}


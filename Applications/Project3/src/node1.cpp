#include "wintundevice.hpp"
#include "argparse/argparse.hpp"

#include "ping.hpp"


inline ByteContainer create_packet(
    const IPV4_addr &src_ip,
    const IPV4_addr &dst_ip,
    const std::uint16_t &identifier,
    const std::uint16_t &seq_num,
    ByteContainer payload
) {

    using boost::asio::detail::socket_ops::host_to_network_short;
    using boost::asio::detail::socket_ops::host_to_network_long;

    IPV4_Header ipv4_header = {
        .ihl = 5,
        .version = 4,
        .tos = 0x00,
        .tlen = host_to_network_short(sizeof(IPV4_Header) + sizeof(ICMP_Header) + payload.size()),
        .id = 0x0000,
        .frag_offset_1 = 0x0000,
        .reserved = 0x00,
        .no_frag = 0x01,
        .more_frag = 0x00,
        .frag_offset_0 = 0x0000,
        .ttl = 0x80,
        .protocol = unsigned(IPV4_Header::Protocol::ICMP),
        .checksum = 0x0000,
        .src = src_ip,
        .dst = dst_ip
    };

    ipv4_header.checksum = checksum(std::span((uint8_t *)&ipv4_header, sizeof(IPV4_Header)));

    ICMP_Header icmp_header = {
        .type = 0x00,
        .code = 0x00,
        .checksum = 0x0000,
        .identifier = identifier,
        .seq_num = seq_num
    };

    icmp_header.checksum = checksum(std::span((uint8_t *)&icmp_header, sizeof(ICMP_Header)), checksum(payload));
    icmp_header.type = unsigned(ICMP_Header::Type::EchoRequest);

    ByteContainer packet;
    packet.push(ipv4_header);
    packet.push(icmp_header);
    packet.insert(packet.end(), payload.begin(), payload.end());

    return packet;

}

int main(int argc, char **argv) {

    auto session = std::make_shared<WinTUN::Device>(
        "node1", GUID { 0xdeadbabe, 0xcafe, 0xbeef, { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef } }
    )->open("172.18.1.3");

    asyncio.run([&]() -> awaitable<void> {

        boost::asio::streambuf buf;

        while (true) {

            ByteContainer payload;
                    
            auto packet = create_packet(
                IPV4_addr("172.18.1.3"),
                IPV4_addr("172.18.1.1"),
                0x0001,
                0x0002,
                payload
            );

            auto p = std::span(boost::asio::buffer_cast<uint8_t *>(buf.prepare(packet.size())), packet.size());
            for (auto i = 0; i < p.size(); i++)
                p[i] = packet[i];
            buf.commit(p.size());

            co_await session->async_send(buf);
            co_await asyncio.sleep(1s);
        }
    }());

    return 0;

}

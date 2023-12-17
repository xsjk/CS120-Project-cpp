#pragma once

#include <iostream>
#include <format>
#include <cstring>
#include <cstdint>
#include <valarray>
#include <array>
#include "sec_api/tchar_s.h"
#include "pcap/pcap.h"
#include <cstdint>
#include <concepts>
#include <thread>
#include <iomanip>
#include <asyncio.hpp>
#include <argparse/argparse.hpp>
#include <utils.hpp>
#include <boost/asio.hpp>
#include <regex>
#include <networkheaders.hpp>


using namespace utils;


inline ByteContainer create_ping_packet(
    const MAC_addr_t &src_mac,
    const MAC_addr_t &dst_mac,
    const IPV4_addr &src_ip,
    const IPV4_addr &dst_ip,
    const std::uint16_t &identifier,
    const std::uint16_t &seq_num
) {

    ByteContainer payload;

    using boost::asio::detail::socket_ops::host_to_network_short;
    using boost::asio::detail::socket_ops::host_to_network_long;

    MAC_Header mac_header = {
        .dst = dst_mac,
        .src = src_mac,
        .type = 0x0008
    };

    IPV4_Header ipv4_header = {
        .ihl = 5,
        .version = 4,
        .tos = 0x00,
        .tlen = host_to_network_short(sizeof(IPV4_Header) + sizeof(ICMP_Header) + payload.size()),
        .id = 0x0000,
        .reserved = 0x00,
        .no_frag = 0x01,
        .more_frag = 0x00,
        .frag_offset = 0x0000,
        .ttl = 0x80,
        .protocal = unsigned(IPV4_Header::Protocal::ICMP),
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
    packet.push(mac_header);
    packet.push(ipv4_header);
    packet.push(icmp_header);
    packet.insert(packet.end(), payload.begin(), payload.end());


    return packet;

}

struct PingPacketConfig {
    MAC_addr_t src_mac;
    MAC_addr_t dst_mac;
    IPV4_addr src_ip;
    IPV4_addr dst_ip;
    std::uint16_t identifier;
    std::uint16_t seq_num;
};
inline def create_ping_packet(PingPacketConfig &&config) {
    return create_ping_packet(config.src_mac, config.dst_mac, config.src_ip, config.dst_ip, config.identifier, config.seq_num);
}


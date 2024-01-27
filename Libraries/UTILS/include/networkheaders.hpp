#pragma once

#include <utils.hpp>
#include <format>

/**
 * @brief calculate the checksum of a container
 *
 * @param container
 * @param last_sum
 * @return uint16_t
 */

template<std::ranges::input_range R>
    requires(std::is_same_v<typename std::remove_reference_t<R>::value_type, uint8_t>)
uint16_t checksum(R &&container, uint16_t last_sum = 0) {
    assert(container.size() % 2 == 0);
    unsigned sum = uint16_t(~last_sum);
    for (auto i = 0; i < container.size(); i += 2) {
        sum += (container[i] << 8) + container[i + 1];
        if (sum > 0xFFFF)
            sum = (sum & 0xFFFF) + 1;
    }
    return ~sum;
}


struct MAC_addr {
    std::uint8_t bytes[6];
    MAC_addr() = default;
    MAC_addr(std::initializer_list<std::uint8_t> list) {
        std::copy(list.begin(), list.end(), bytes);
    }
    MAC_addr(const char *str) {
        if (std::sscanf(str, "%02hhx-%02hhx-%02hhx-%02hhx-%02hhx-%02hhx", bytes, bytes + 1, bytes + 2, bytes + 3, bytes + 4, bytes + 5) != 6
            && std::sscanf(str, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", bytes, bytes + 1, bytes + 2, bytes + 3, bytes + 4, bytes + 5) != 6)
            throw std::runtime_error("invalid MAC address");
    }
    MAC_addr(std::string str) : MAC_addr(str.c_str()) { }
    auto operator[](std::size_t i) const { return bytes[i]; }
    operator std::string() const {
        return std::format("{:02X}-{:02X}-{:02X}-{:02X}-{:02X}-{:02X}", bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
    }
};

static_assert(sizeof(MAC_addr) == 6);


union IPV4_addr {
    IPV4_addr() : addr(0) { }
    IPV4_addr(std::uint32_t addr) : addr(addr) { }
    std::uint32_t addr;
    std::uint8_t bytes[4];
    IPV4_addr(std::initializer_list<std::uint8_t> list) {
        std::copy(list.begin(), list.end(), bytes);
    }
    IPV4_addr(const char *str) {
        if (std::sscanf(str, "%hhu.%hhu.%hhu.%hhu", bytes, bytes + 1, bytes + 2, bytes + 3) != 4)
            throw std::runtime_error("invalid IPv4 address");
    }
    constexpr IPV4_addr(int a, int b, int c, int d)
     : bytes{ (std::uint8_t)a, (std::uint8_t)b, (std::uint8_t)c, (std::uint8_t)d } { }
    IPV4_addr(std::string str) : IPV4_addr(str.c_str()) { }
    operator std::string() const {
        return std::format("{:d}.{:d}.{:d}.{:d}", (int)bytes[0], (int)bytes[1], (int)bytes[2], (int)bytes[3]);
    }
    operator std::uint32_t() const {
        return addr;
    }
    std::uint8_t &operator[](size_t i) {
        return bytes[i];
    }
    const std::uint8_t &operator[](size_t i) const {
        return bytes[i];
    }
    auto operator<=>(const IPV4_addr &other) const {
        return addr <=> other.addr;
    }
};

static_assert(sizeof(IPV4_addr) == 4);

struct MAC_Header {
    MAC_addr dst;
    MAC_addr src;
    std::uint16_t type = 0x0008;
    enum class Type : uint16_t {
        IPv4 = 0x0008,
        ARP = 0x0608,
        IPv6 = 0xDD86
    };
};

static_assert(sizeof(MAC_Header) == 14);



/* IPv4 header */
struct IPV4_Header {
    unsigned ihl : 4 = 5;       // unsignedernet header length
    unsigned version : 4 = 4;   // version
    unsigned tos : 8;           // type of service
    unsigned tlen : 16;         // header and payload total length
    unsigned id : 16;           // identification
    unsigned frag_offset_1 : 5; // fragment offset
    unsigned reserved : 1;      // reserved bit
    unsigned no_frag : 1;       // no fragment
    unsigned more_frag : 1;     // more fragment
    unsigned frag_offset_0 : 8; // fragment offset
    unsigned ttl : 8;           // time to live
    unsigned protocol : 8;      // protocol
    enum class Protocol {
        ICMP = 1, IGMP = 2, IP = 4, TCP = 6, IPv6 = 41, UDP = 17
    };
    unsigned checksum : 16;     // checksum
    unsigned src;               // source address
    unsigned dst;               // destination address
};

static_assert(sizeof(IPV4_Header) == 20);


struct ICMP_Header {

    unsigned type : 8;
    unsigned code : 8;
    unsigned checksum : 16;
    unsigned identifier : 16;
    unsigned seq_num : 16;

    enum class Type : uint8_t {
        EchoReply = 0,
        DestinationUnreachable = 3,
        SourceQuench = 4,
        Redirect = 5,
        EchoRequest = 8,
        TimeExceeded = 11,
        ParameterProblem = 12,
        Timestamp = 13,
        TimestampReply = 14,
        InformationRequest = 15,
        InformationReply = 16,
        AddressMaskRequest = 17,
        AddressMaskReply = 18
    };

};

static_assert(sizeof(ICMP_Header) == 8);

/* TCP header */
struct TCP_Header {
    unsigned src_port : 16;         // Source port
    unsigned dst_port : 16;         // Destination port
    unsigned seq_num;               // Sequence number
    unsigned ack_num;               // Acknowledgment number
    unsigned aec : 1;               // Accurate ECN
    unsigned reserved : 3;          // Reserved for future use (must be zero)
    unsigned length : 4;            // Header length / 4
    unsigned fin : 1;               // No more data from sender
    unsigned syn : 1;               // Synchronize sequence numbers
    unsigned rst : 1;               // Reset the connection
    unsigned psh : 1;               // Push function
    unsigned ack : 1;               // Acknowledgment field significant
    unsigned urg : 1;               // Urgent pointer field significant
    unsigned ece : 1;               // ECN-Echo
    unsigned cwr : 1;               // Congestion window reduced
    unsigned window : 16;           // Window size
    unsigned checksum : 16;         // Checksum
    unsigned urgent : 16;           // Urgent pointer

    // Options (if data_offset > 5, not included in this struct)
    // unsigned options[...];
};

static_assert(sizeof(TCP_Header) == 20);


/* Pseudo header */
struct Pseudo_Header {
    unsigned src;
    unsigned dst;
    unsigned zero : 8;
    unsigned protocol : 8;
    unsigned length : 16;
};

static_assert(sizeof(Pseudo_Header) == 12);

template<typename Header, typename Payload>
struct Packet {
    Header header;  // the header struct will be send as byte
    std::shared_ptr<Payload> payload;

    constexpr std::size_t size() {
        if (payload)
            return sizeof(header);
        else
            return sizeof(header) + payload->size();
    }

    auto to_bytes(boost::asio::streambuf buffer) {
        auto n = size();
        to_bytes(boost::asio::buffer_cast<uint8_t *>(buffer.prepare(n)));
    }

    auto to_bytes() -> utils::ByteContainer {
        utils::ByteContainer container;
        to_bytes(container);
        return container;
    }

private:

    auto to_bytes(utils::ByteContainer &container) {
        container.push(header);
        if (payload)
            if constexpr (requires { payload->to_bytes(container); })
                payload->to_bytes(container);
            else
                container.insert(payload->begin(), payload->end());
    }

    auto to_bytes(uint8_t *p) {
        std::memcpy(p, &header, sizeof(header));
        p += sizeof(header);
        if (payload)
            if constexpr (requires { payload->to_bytes(p); })
                payload->to_bytes(p);
            else
                std::memcpy(p, payload->data(), payload->size());
    }
};

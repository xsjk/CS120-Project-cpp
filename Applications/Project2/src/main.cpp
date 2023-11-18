#include "physical.hpp"
#include "callbacklayer.hpp"
#include "CRC.hpp"

#include <span>
#include <ranges>
#include <unordered_map>
#include <cmath>

namespace Physical {

    auto config = BitStreamDeviceConfig {
        .package_size = 8,
        .preamble = StaticPreamble("preamble.txt", 150),
        .modem = DigitalModem(10)
    };

    Sender sender { config.modem, config.preamble, config.package_size };
    Receiver receiver { config.modem, config.preamble, config.package_size };

}

// OSI model layer 1: Physical layer
// the layer to convert sound signal to bit stream
struct PhysicalLayerHandler {
    using LowerData = DataView<float>;
    using UpperData = BitsContainer;

    /* | preamble | data  | reserved | 
       | fixed    | fixed | fixed    |
     */

    UpperData passUpper(LowerData &&input)  {
        Physical::receiver.handleCallback(input);
        BitsContainer output;
        if (Physical::receiver.available())
            output = Physical::receiver.read();
        return output;
    }

    void passLower(UpperData &&input, LowerData &output) {
        Physical::sender.send(std::move(input));
        Physical::sender.handleCallback(output);
    }
};


// OSI model layer 2: Data link layer
// the layer to convert bit stream to frame of bytes
// template<
//     uint8_t frame_delimiter = 0b11010101,
//     uint8_t escape_character = 0b01011100
// >
struct DataLinkFrameHandler {
    using LowerData = BitsContainer;
    using UpperData = std::vector<ByteContainer>;

    CRC8<0x7> crc_checker = {};
    static constexpr uint8_t frame_delimiter = 0b11010101; // the start or end of a frame
    static constexpr uint8_t escape_character = 0b01011100; // the escape character (use the ascii code of r'\' here)
                                                            // when the frame_delimiter or escape_character appears in the frame,
                                                            // the escape_character will be added before it to avoid ambiguity

    /* | frame_delimiter | data frame | crc | frame_delimiter |
     * | 1 B             | ...        | 1 B | 1 B             |
    */
    ByteContainer frameBuffer; // the received frame from upper layer, not complete yet
    bool escape = false; // whether the next byte is escaped

    // the input is some bits in a frame, maybe not a complete frame
    UpperData passUpper(LowerData &&input) {
        if (input.size() % CHAR_BIT != 0)
            // The physical layer should guarantee that the bit stream is aligned to byte
            throw std::runtime_error("DataLinkLayerHandler::passUpper: input size must be a multiple of CHAR_BIT");

        using CharSpan = std::span<const uint8_t>;

        UpperData output;

        for (auto byte : input.as_span<uint8_t>()) {
            if (escape == false) {
                if (byte == escape_character) {
                    escape = true;
                } else if (byte == frame_delimiter) {
                    // the start or end of a frame
                    if (frameBuffer.size() > 0) {
                        // the end of a frame
                        bool crc_ok = crc_checker.check(frameBuffer);
                        if (crc_ok) {
                            // frame complete!
                            // pass the frame to upper layer and clear the buffer
                            output.push_back(std::move(frameBuffer));
                        } else {
                            // ignore the frame 
                            std::cout << "CRC error" << std::endl;
                            frameBuffer.clear();
                        }
                    } else {
                        // the start of a frame
                        frameBuffer.push_back(byte);
                    }
                } else {
                    // normal byte
                    frameBuffer.push_back(byte);
                }
            } else {
                // normal byte
                frameBuffer.push_back(byte);
                escape = false;
            }
        }

        return output;
    }

    LowerData passLower(std::vector<ByteContainer> &&input) {

        LowerData output;

        for (auto &&frame : input) {
            output.push(frame_delimiter);
            auto crc = crc_checker.get(frame);
            for (auto byte : frame) {
                if (byte == frame_delimiter || byte == escape_character)
                    output.push(escape_character);
                output.push(byte);
            }
            output.push(crc);
            output.push(frame_delimiter);

        }

        return output;

    }
};



template<class H, class T>
struct Packet {
    H header;
    T data;
};



using MACAddr = uint8_t; // the type of MAC address
using IPAddr = uint8_t; // the type of IP address

constexpr MACAddr my_mac = 0x01;
constexpr IPAddr my_ip = 0x01;


struct MACLayerHandler {

    using LowerData = std::vector<ByteContainer>;
    using UpperData = std::vector<std::span<const uint8_t>>;
    
    /* | Destination MAC | Source MAC | Type | Data |
     * | 1 B             | 1 B        | 1 B  | ...  |
    */
    struct Header {
        MACAddr destination;
        MACAddr source;
        enum class Type : uint8_t {
            ARP = 0x01,
            IP = 0x02,
        } type;
    };

    
    struct ARP {
        enum class Operation : uint8_t {
            Request = 0x01,
            Reply = 0x02
        } operation;
        IPAddr sender_ip;
        IPAddr target_ip;
    };

    std::unordered_map<MACAddr, IPAddr> arp_table;

    static constexpr MACAddr broadcast_address = 0xff;

    LowerData frameBuffer; // the data to be sent to lower layer, not complete yet

    UpperData passUpper(const LowerData &input) {

        UpperData output;

        for (const auto& frame: input) {

            if (frame.size() < sizeof(Header)) {
                // the frame is too short
                std::cerr << "MACLayerHandler::passUpper: the frame is too short" << std::endl;
                continue;
            }
            auto header = (Header*)frame.data();
            if (header->destination != broadcast_address && header->destination != my_mac) {
                // the frame is not for me
                std::cout << "MACLayerHandler::passUpper: the frame is not for me (MAC)" << std::endl;
                continue;
            }

            switch (header->type) {
                case Header::Type::ARP:
                    {
                        auto data = (ARP*)(header + 1);
                        switch (data->operation) {
                            case ARP::Operation::Request:
                                {
                                    if (data->target_ip == my_ip) {
                                        // reply
                                        std::cout << "ARP request" << std::endl;

                                        frameBuffer.emplace_back();
                                        frameBuffer.back().push( Packet {
                                            .header = Header {
                                                .destination = header->source,
                                                .source = my_mac,
                                                .type = Header::Type::ARP
                                            },
                                            .data = ARP {
                                                .operation = ARP::Operation::Reply,
                                                .sender_ip = my_ip,
                                                .target_ip = data->sender_ip
                                            }
                                        });
                                    }
                                }
                                break;
                            case ARP::Operation::Reply:
                                {
                                    if (data->target_ip == my_ip) {
                                        // update the arp table
                                        arp_table[data->sender_ip] = header->source;
                                        std::cout << "ARP reply" << std::endl;
                                    }
                                }
                                break;
                            default:
                                // unknown operation
                                std::cerr << "MACLayerHandler::passUpper: unknown operation" << std::endl;
                                continue;
                        }

                    }
                    break;
                case Header::Type::IP: 
                    output.push_back(std::span(frame).subspan(sizeof(Header)));
                    break;
                default:
                    // unknown type
                    std::cerr << "MACLayerHandler::passUpper: unknown type" << std::endl;
                    break;
            }
        }
        return {};
    }

    std::vector<ByteContainer> passLower(std::vector<ByteContainer> &&input) {

        for (auto&& frame: input) 
            if (frame.size() > 0) {
                // send the IP packet
                frame.add_header(Header {
                    .destination = arp_table[my_ip],
                    .source = my_mac,
                    .type = Header::Type::IP
                });
                frameBuffer.emplace_back(std::move(frame));
            }
        return std::move(frameBuffer);
    }

};

// OSI model layer 3: Network layer
// the layer to convert frame of bytes to packet of bytes
struct NetworkLayerHandler {
    using LowerData = std::vector<std::span<const uint8_t>>;

    using IPAddr = uint8_t; // the type of IP address
    
    struct Header {
        IPAddr source_ip;
        IPAddr destination_ip;
    };
    
    using UpperData = std::vector<std::span<const uint8_t>>;

    UpperData passUpper(LowerData &&input)  {

        UpperData output;

        for (auto&& frame: input) {
            if (frame.size() < sizeof(Header)) {
                // the packet is too short
                std::cerr << "NetworkLayerHandler::passUpper: the packet is too short" << std::endl;
                continue;
            }
            auto ip = (Header*)frame.data();
            if (ip->destination_ip != my_ip) {
                // the packet is not for me
                std::cout << "NetworkLayerHandler::passUpper: the packet is not for me (IP)" << std::endl;
                continue;
            }

            output.push_back(frame.subspan(sizeof(Header)));
        }
        return output;
    }

    std::vector<ByteContainer> passLower(std::vector<ByteContainer> &&input) {
        // calculate the output of this layer based on the ouput of upper layer
        // upper_output -> ouput
        for (auto &packet: input) {
            if (packet.size() > 0) {
                packet.add_header(Header {
                    .source_ip = my_ip,
                    .destination_ip = my_ip,
                });
            }
        }
        return std::move(input);
    }
};


// OSI model layer 4: Transport layer
// the layer 
struct TransportLayerHandler {

    using LowerData = std::vector<std::span<const uint8_t>>;
    using UpperData = ByteContainer;


    enum class Protocol : uint8_t {
        SlidingWindow = 0x01,
        TCP = 0x02,
        UDP = 0x03
    };

    struct SlidingWindowHeader {
        bool is_ack: 1;
        uint8_t seq_num: 7;
    };

    struct TCPHeader {
        uint8_t src_port;
        uint8_t dest_port;
        uint8_t seq_num;
        uint8_t ack_num;
        /// TODO: add more fields
    };

    struct UDPHeader {
        uint8_t src_port;
        uint8_t dest_port;
        uint8_t length;   // the length including the header

    };

    static constexpr int window_size = 8;



    
    std::vector<ByteContainer> dataToSend, ackToSend;
    int seq_counter = 0;
    int LFS = 0;
    int LAR = 0;
    int LFR = 0;

    int expect_seq_num = 0;
    int last_ack_pos = -1; 
    int last_succussful_receive = 0; 

    void sendAck(int seq_num) {
        ackToSend.emplace_back();
        ackToSend.back().push( SlidingWindowHeader {
            .is_ack = true,
            .seq_num = (uint8_t)(seq_num),
        });
    }

    constexpr static std::chrono::milliseconds timeout = std::chrono::milliseconds(200);
    std::chrono::time_point<std::chrono::steady_clock> timer_start_time;
    bool is_time_out() {
        return std::chrono::steady_clock::now() - timer_start_time > timeout;
    }
    void timer_start() {
        timer_start_time = std::chrono::steady_clock::now();
    }

    UpperData passUpper(LowerData &&input)  {
        // input from lower

        std::vector<std::span<const uint8_t>> output;

        for (auto &&packet: input) {
            if (packet.size() < 1) 
                continue;
            auto protocol = Protocol(packet[0]);
            packet = packet.subspan(1);
            switch (protocol) {
                case Protocol::SlidingWindow:
                    {
                        if (packet.size() < sizeof(SlidingWindowHeader))
                            continue;
                        auto header = (SlidingWindowHeader*)packet.data();
                        if (!is_time_out())
                            if (header->is_ack) {
                                // sender receive ack
                                std::cout << "SlidingWindow ack: " << (int)header->seq_num << std::endl;
                                if (header->seq_num > last_ack_pos && header->seq_num <= last_ack_pos + window_size) {
                                    last_ack_pos = header->seq_num;
                                    timer_start();
                                }
                                
                            } else {
                                // receiver receive data
                                std::cout << "SlidingWindow data: " << (int)header->seq_num << std::endl;
                                if (header->seq_num == last_succussful_receive + 1)
                                    last_succussful_receive++;
                                // send ack
                                sendAck(header->seq_num);
                            }
                    }
                    break;
                case Protocol::TCP:
                    {
                        if (packet.size() < sizeof(TCPHeader))
                            continue;
                        auto header = (TCPHeader*)packet.data();
                        /// TODO: handle the TCP packet
                    }
                    break;
                case Protocol::UDP:
                    {
                        if (packet.size() < sizeof(UDPHeader))
                            continue;
                        auto header = (UDPHeader*)packet.data();

                        output.push_back(packet.subspan(sizeof(UDPHeader)));
                    }
                    break;
                default:
                    std::cerr << "TransportLayerHandler::passUpper: unknown type" << std::endl;
                    continue;
            }
        }
        return {};
    }

    

    std::vector<ByteContainer> passLower(ByteContainer &&input) {
        // split the input into packets and add the header
        for (auto it = input.begin(); it != input.end(); it += window_size) {
            auto end = std::min(it + window_size, input.end());
            dataToSend.emplace_back();
            dataToSend.back().push( Packet {
                .header = SlidingWindowHeader {
                    .is_ack = false,
                    .seq_num = (uint8_t)(seq_counter++),
                },
                .data = ByteContainer(it, end)
            });
        }

        std::vector<ByteContainer> output;
        
        // decide the old ack


        // send the acks
        for (auto&& ack: ackToSend)
            output.push_back(std::move(ack));
        ackToSend.clear();

        
        if (is_time_out()) {
            // resend the window
            for (int i = last_ack_pos + 1; i < std::min(
                last_ack_pos + 1 + window_size,
                (int)dataToSend.size()
            ); i++) {
                output.push_back(dataToSend[i]);
            }
            timer_start();
        }
        return {};
    }
};


// OSI model layer 5: Session layer
// the layer
struct SessionLayerHandler {

    auto passUpper(auto &&input)  {
        // calculate the output of this layer based on the ouput of lower layer
        // lower_output -> ouput
        return std::move(input);
    }

    auto passLower(auto &&input) {
        // calculate the output of this layer based on the ouput of upper layer
        // upper_output -> ouput
        return std::move(input);
    }
};


// OSI model layer 6: Presentation layer
// the layer
struct PresentationLayerHandler {

    auto passUpper(auto &&input)  {
        // calculate the output of this layer based on the ouput of lower layer
        // lower_output -> ouput
        return std::move(input);
    }

    auto passLower(auto &&input) {
        // calculate the output of this layer based on the ouput of upper layer
        // upper_output -> ouput
        return std::move(input);
    }
};


// OSI model layer 7: Application layer
// the layer
struct ApplicationLayerHandler {
    using LowerData = ByteContainer;
    using UpperData = void;

    void passUpper(LowerData &&input) {
        for (auto c : input)
            std::cout << c;
    }
    LowerData passLower() {
        return {};
    }
};


#include <fstream>
struct FileIOLayer {
    using LowerData = ByteContainer;
    using UpperData = void;

    std::ifstream fin { "INPUT.bin" , std::ios::binary };
    std::ofstream fout { "OUTPUT.bin" , std::ios::binary };

    void passUpper(LowerData &&input) {
        for (auto c : input)
            std::cout << c;
        // write to file
        fout.write((char*)input.data(), input.size());
    }

    LowerData passLower() {
        LowerData output;
        while (fin) {
            char c;
            fin >> c;
            output.push(c);
        }
        return output;
    }


};


int main() {

    auto device = std::make_shared<Device>();
    auto io = std::make_shared<OSI::MultiLayerIOHandler<
        PhysicalLayerHandler,
        DataLinkFrameHandler,
        MACLayerHandler,
        NetworkLayerHandler,
        TransportLayerHandler,
        SessionLayerHandler,
        PresentationLayerHandler,
        FileIOLayer
    >>();
    device->open();
    device->start(io);

    std::this_thread::sleep_for(std::chrono::seconds(10));
    return 0;

}

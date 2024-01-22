#include <iostream>
#include <pcap/pcap.h>
#include <networkheaders.hpp>
#include <map>
#include <utility>
#include <set>
#include <utils.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <time.h>
#include <tchar.h>
#include <ping.hpp>

using boost::asio::detail::socket_ops::host_to_network_short;

using port_t = std::uint16_t;

IPV4_addr WAN_IP = "10.20.166.64";
IPV4_addr LAN_IP = "192.168.137.1";

std::map<port_t, std::pair<IPV4_addr, port_t>> NAT_table;
port_t port = 0;
std::set<IPV4_addr> LAN_IPs = { };
pcap_t *handle;
pcap_t *wlan_handle;



std::map<IPV4_addr, MAC_addr> wlan_ip_to_mac = {
    {{"10.20.166.64"}, {"BC-17-B8-30-39-B9"}},
    {{"1.1.1.1"},      {"00-00-5e-00-01-01"}}
};

std::map<IPV4_addr, MAC_addr> hotspot_ip_to_mac = {
    {{"192.168.137.1"},   {"BE-17-B8-30-39-B9"}},
    {{"192.168.137.168"}, {"92-fd-83-21-e8-97"}}
};



void packet_handler(u_char *user, const struct pcap_pkthdr *header, const u_char *packet) {
    // Check if it is an ICMP packet and forward it to wlan interface if needed

    auto mac_header = reinterpret_cast<const MAC_Header*>(packet);
    if (mac_header->type == static_cast<unsigned>(MAC_Header::Type::IPv4)) {
        auto ip_header = reinterpret_cast<const IPV4_Header*>(packet + sizeof(MAC_Header));
        if (ip_header->protocol == static_cast<unsigned>(IPV4_Header::Protocol::ICMP)) {

            auto length = header->len;
            auto packet_copy = utils::ByteContainer(packet, packet + length);
            auto mac_header = reinterpret_cast<MAC_Header*>(packet_copy.data());
            auto ip_header = reinterpret_cast<IPV4_Header*>(packet_copy.data() + sizeof(MAC_Header));
            auto icmp_header = reinterpret_cast<ICMP_Header*>(packet_copy.data() + sizeof(MAC_Header) + sizeof(IPV4_Header));
            auto icmp_payload_size = length - sizeof(MAC_Header) - sizeof(IPV4_Header) - sizeof(ICMP_Header);


            if (icmp_header->type == static_cast<unsigned>(ICMP_Header::Type::EchoRequest)) {

                if (ip_header->dst == IPV4_addr("1.1.1.1").addr &&
                    ip_header->src != WAN_IP.addr) {

                    std::cout << std::format(
                        "Ping request from {} to {}",
                        std::string(IPV4_addr(ip_header->src)),
                        std::string(IPV4_addr(ip_header->dst))
                    ) << std::endl;

                    // save NAT translation
                    port_t id = port++;
                    // IPV4_addr dst = ip_header.dst;
                    IPV4_addr src (ip_header->src);
                    unsigned short old_id = icmp_header->identifier;
                    auto pair = std::make_pair(src, old_id);
                    NAT_table[id] = pair;

					ip_header->id = 0;
                    ip_header->src = WAN_IP.addr;
                    ip_header->checksum = 0;
                    ip_header->checksum = checksum(std::span((uint8_t *)ip_header, sizeof(IPV4_Header)));
                    ip_header->checksum = host_to_network_short(ip_header->checksum);

                    icmp_header->checksum = 0;
                    icmp_header->identifier = id;
                    icmp_header->checksum = checksum(std::span((uint8_t *)icmp_header, sizeof(ICMP_Header) + icmp_payload_size));
                    icmp_header->checksum = host_to_network_short(icmp_header->checksum);

                    mac_header->src = wlan_ip_to_mac.at(WAN_IP);
                    mac_header->dst = wlan_ip_to_mac.at("1.1.1.1");

                    std::cout << "send packet to wlan: \n" << packet_copy << std::endl;

                    pcap_sendpacket(wlan_handle, packet_copy.data(), packet_copy.size());
                } else {
                    // already to WAN
                }

            }
        }
    }
}

void wlan_packet_handler(u_char *user, const struct pcap_pkthdr *header, const u_char *packet) {
    // Check if it is an ICMP packet and forward it to WAN interface if needed

    auto mac_header = reinterpret_cast<const MAC_Header*>(packet);
    if (mac_header->type == static_cast<unsigned>(MAC_Header::Type::IPv4)) {

        auto ip_header = reinterpret_cast<const IPV4_Header*>(packet + sizeof(MAC_Header));
        if (ip_header->protocol == static_cast<unsigned>(IPV4_Header::Protocol::ICMP)) {

            auto length = header->len;
            auto packet_copy = utils::ByteContainer(packet, packet + length);
            auto mac_header = reinterpret_cast<MAC_Header*>(packet_copy.data());
            auto ip_header = reinterpret_cast<IPV4_Header*>(packet_copy.data() + sizeof(MAC_Header));
            auto icmp_header = reinterpret_cast<ICMP_Header*>(packet_copy.data() + sizeof(MAC_Header) + sizeof(IPV4_Header));
            auto icmp_payload_size = length - sizeof(MAC_Header) - sizeof(IPV4_Header) - sizeof(ICMP_Header);

            if (icmp_header->type == static_cast<unsigned>(ICMP_Header::Type::EchoReply)) {

                // reverse NAT translation
                port_t id = icmp_header->identifier;

                if (ip_header->src == IPV4_addr("1.1.1.1").addr &&
                    ip_header->dst == WAN_IP.addr &&
                    NAT_table.find(id) != NAT_table.end()) {

                    std::cout << std::format(
                        "Ping reply from {} to {}",
                        std::string(IPV4_addr(ip_header->src)),
                        std::string(IPV4_addr(ip_header->dst))
                    ) << std::endl;

                    auto [dst, old_id] = NAT_table[id];
                    ip_header->dst = dst;
                    ip_header->checksum = 0;
                    ip_header->checksum = checksum(std::span((uint8_t *)ip_header, sizeof(IPV4_Header)));
                    ip_header->checksum = host_to_network_short(ip_header->checksum);

                    icmp_header->identifier = old_id;
                    icmp_header->checksum = 0;
                    icmp_header->checksum = checksum(std::span((uint8_t *)icmp_header, sizeof(ICMP_Header) + icmp_payload_size));
                    icmp_header->checksum = host_to_network_short(icmp_header->checksum);

                    mac_header->src = hotspot_ip_to_mac.at(LAN_IP);
                    mac_header->dst = hotspot_ip_to_mac.at(dst);

                    std::cout << "send packet to hotspot: \n" << packet_copy << std::endl;

                    pcap_sendpacket(handle, packet_copy.data(), packet_copy.size());
                }
            }
        }
    }
}

BOOL LoadNpcapDlls()
{
	_TCHAR npcap_dir[512];
	UINT len;
	len = GetSystemDirectory(npcap_dir, 480);
	if (!len) {
		fprintf(stderr, "Error in GetSystemDirectory: %x", GetLastError());
		return FALSE;
	}
	_tcscat_s(npcap_dir, 512, _T("\\Npcap"));
	if (SetDllDirectory(npcap_dir) == 0) {
		fprintf(stderr, "Error in SetDllDirectory: %x", GetLastError());
		return FALSE;
	}
	return TRUE;
}

int main()
{
    pcap_if_t *alldevs;
    pcap_if_t *d;
    int inum;
    int i=0;
    char errbuf[PCAP_ERRBUF_SIZE];

    /* Load Npcap and its functions. */
    if (!LoadNpcapDlls())
    {
        fprintf(stderr, "Couldn't load Npcap\n");
        exit(1);
    }

    /* Retrieve the device list */
    if(pcap_findalldevs(&alldevs, errbuf) == -1)
    {
        fprintf(stderr,"Error in pcap_findalldevs: %s\n", errbuf);
        exit(1);
    }

    /* Print the list */
    for(d=alldevs; d; d=d->next)
    {
        printf("%d. %s", ++i, d->name);
        if (d->description)
            printf(" (%s)\n", d->description);
        else
            printf(" (No description available)\n");
    }

    if(i==0)
    {
        printf("\nNo interfaces found! Make sure Npcap is installed.\n");
        return -1;
    }

    printf("Enter the interface number (hotspot wlan) ");
    scanf("%d", &inum);

    /* Jump to the selected adapter */
    for(d=alldevs, i=0; i< inum-1 ;d=d->next, i++);

    /* Open the device */
    /* Open the adapter */
    if ((handle= pcap_open_live(d->name,    // name of the device
                             65536,         // portion of the packet to capture.
                                            // 65536 grants that the whole packet will be captured on all the MACs.
                             1,             // promiscuous mode (nonzero means promiscuous)
                             10,            // read timeout
                             errbuf         // error buffer
                             )) == NULL)
    {
        fprintf(stderr,"\nUnable to open the adapter. %s is not supported by Npcap\n", d->name);
        /* Free the device list */
        pcap_freealldevs(alldevs);
        return -1;
    }

    scanf("%d", &inum);

    /* Jump to the selected adapter */
    for(d=alldevs, i=0; i< inum-1 ;d=d->next, i++);

    /* Open the device */
    /* Open the adapter */
    if ((wlan_handle= pcap_open_live(d->name,    // name of the device
                             65536,            // portion of the packet to capture.
                                            // 65536 grants that the whole packet will be captured on all the MACs.
                             1,                // promiscuous mode (nonzero means promiscuous)
                             10,            // read timeout
                             errbuf            // error buffer
                             )) == NULL)
    {
        fprintf(stderr,"\nUnable to open the adapter. %s is not supported by Npcap\n", d->name);
        /* Free the device list */
        pcap_freealldevs(alldevs);
        return -1;
    }




    struct bpf_program fcode;

    //compile the filter
    if (pcap_compile(handle, &fcode, "icmp", 1, 0xffffff) <0 )
    {
        fprintf(stderr,"\nUnable to compile the packet filter. Check the syntax.\n");
        /* Free the device list */
        pcap_freealldevs(alldevs);
        return -1;
    }

    //set the filter
    if (pcap_setfilter(handle, &fcode)<0)
    {
        fprintf(stderr,"\nError setting the filter.\n");
        /* Free the device list */
        pcap_freealldevs(alldevs);
        return -1;
    }


    struct bpf_program wan_fcode;

    //compile the filter
    if (pcap_compile(wlan_handle, &wan_fcode, "icmp", 1, 0xffffff) <0 )
    {
        fprintf(stderr,"\nUnable to compile the packet filter. Check the syntax.\n");
        /* Free the device list */
        pcap_freealldevs(alldevs);
        return -1;
    }

    //set the filter
    if (pcap_setfilter(wlan_handle, &wan_fcode)<0)
    {
        fprintf(stderr,"\nError setting the filter.\n");
        /* Free the device list */
        pcap_freealldevs(alldevs);
        return -1;
    }


    printf("\nlistening on %s...\n", d->description);

    /* At this point, we don't need any more the device list. Free it */
    pcap_freealldevs(alldevs);

    /* start the capture */
    std::jthread hotspot_listen_thread ([](){
        pcap_loop(handle, 0, packet_handler, NULL);
    });
    std::jthread wlan_listen_thread ([](){
        pcap_loop(wlan_handle, 0, wlan_packet_handler, NULL);
    });
    wlan_listen_thread.join();
    hotspot_listen_thread.join();

    pcap_close(handle);
    return 0;
}
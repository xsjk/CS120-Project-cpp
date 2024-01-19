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

port_t port = 0;
pcap_t *handle;
pcap_t *wlan_handle;




void packet_handler(u_char *user, const struct pcap_pkthdr *header, const u_char *packet) {
    // Check if it is an ICMP packet and forward it to wlan interface if needed
    auto ip_header = reinterpret_cast<const IPV4_Header*>(packet);
    if (ip_header->protocal == static_cast<unsigned>(IPV4_Header::Protocal::ICMP)) {

        auto length = header->len;
        auto packet_copy = utils::ByteContainer(packet, packet + length);
        auto ip_header = reinterpret_cast<IPV4_Header*>(packet_copy.data());
        auto icmp_header = reinterpret_cast<ICMP_Header*>(packet_copy.data() + sizeof(IPV4_Header));
        auto icmp_payload_size = length - sizeof(IPV4_Header) - sizeof(ICMP_Header);

        IPV4_addr src_ip(ip_header->src);
        IPV4_addr dst_ip(ip_header->dst);

        if (src_ip.bytes[0] == 172 &&
            src_ip.bytes[1] == 18 &&
            src_ip.bytes[2] == 2 &&
            dst_ip.bytes[0] == 172 &&
            dst_ip.bytes[1] == 18 &&
            dst_ip.bytes[2] != 2
            )
        {
            MAC_Header mac_header = {
                .dst = "50-33-f0-e2-1e-dc",
                .src = "BC-17-B8-30-39-B9",
                .type = 0x0008
            };

            packet_copy.insert(packet_copy.begin(), (uint8_t *)&mac_header, (uint8_t *)&mac_header + sizeof(MAC_Header));
            pcap_sendpacket(wlan_handle, packet_copy.data(), packet_copy.size());
        }
    }
}

void wlan_packet_handler(u_char *user, const struct pcap_pkthdr *header, const u_char *packet) {
    // Check if it is an ICMP packet and forward it to WAN interface if needed

    auto mac_header = reinterpret_cast<const MAC_Header*>(packet);
    if (mac_header->type == static_cast<unsigned>(MAC_Header::Type::IPv4)) {

        auto ip_header = reinterpret_cast<const IPV4_Header*>(packet + sizeof(MAC_Header));
        if (ip_header->protocal == static_cast<unsigned>(IPV4_Header::Protocal::ICMP)) {

            auto length = header->len;
            auto packet_copy = utils::ByteContainer(packet, packet + length);
            auto mac_header = reinterpret_cast<MAC_Header*>(packet_copy.data());
            auto ip_header = reinterpret_cast<IPV4_Header*>(packet_copy.data() + sizeof(MAC_Header));
            auto icmp_header = reinterpret_cast<ICMP_Header*>(packet_copy.data() + sizeof(MAC_Header) + sizeof(IPV4_Header));
            auto icmp_payload_size = length - sizeof(MAC_Header) - sizeof(IPV4_Header) - sizeof(ICMP_Header);

            IPV4_addr src_ip(ip_header->src);
            IPV4_addr dst_ip(ip_header->dst);

            if (src_ip.bytes[0] == 172 &&
                src_ip.bytes[1] == 18 &&
                src_ip.bytes[2] != 2 &&
                dst_ip.bytes[0] == 172 &&
                dst_ip.bytes[1] == 18 &&
                dst_ip.bytes[2] == 2
                ) 
            {
                pcap_sendpacket(handle, packet_copy.data() + sizeof(MAC_Header), packet_copy.size() - sizeof(MAC_Header));
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
                             100,            // read timeout
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
                             100,            // read timeout
                             errbuf            // error buffer
                             )) == NULL)
    {
        fprintf(stderr,"\nUnable to open the adapter. %s is not supported by Npcap\n", d->name);
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



    auto packet = create_ping_request({
        .src_mac = "80-45-dd-e5-c0-f9",
        .dst_mac = "bc-17-b8-30-39-b9",
        .src_ip = "192.168.43.231",
        .dst_ip = "172.18.2.3",
        .identifier = 0x0000,
        .seq_num = 0x0000,
    });

    pcap_sendpacket(wlan_handle, packet.data(), packet.size());

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
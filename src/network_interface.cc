#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.

void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
    uint32_t dst_ip = dgram.header.dst;
    EthernetAddress dst_eth;
    if(!ARP_cache.getEntry(dst_ip, dst_eth, currentTimeStamp)){
        EthernetFrame ARPRequest {
                .header = {
                        .dst = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
                        .src = ethernet_address_,
                        .type = EthernetHeader::TYPE_ARP
                },
//                .payload = ;
        };
        transmit(ARPRequest);
        sendQueue.insert({dst_ip, dgram});
        return;
    }
    EthernetFrame dataFrame {
            .header = {
                    .dst = dst_eth,
                    .src = ethernet_address_,
                    .type = EthernetHeader::TYPE_IPv4
            },
            .payload = serialize(dgram)
    };
    (void) next_hop;


    transmit(dataFrame);
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // Your code here.
  (void)frame;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
    currentTimeStamp += ms_since_last_tick;
}

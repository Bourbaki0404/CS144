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
    EthernetAddress dst_eth;
    if(!ARP_cache.getEntry(next_hop.ipv4_numeric(), dst_eth, currentTimeStamp)){
        if(!sendQueue.contains(next_hop.ipv4_numeric()))
            sendQueue.insert({next_hop.ipv4_numeric(), {dgram}});
        else
            sendQueue[next_hop.ipv4_numeric()].push_back(dgram);
        if(!(ARPResendQueue.contains(next_hop.ipv4_numeric()) &&
        currentTimeStamp - ARPResendQueue[next_hop.ipv4_numeric()] <= resend_interval)){
            ARPMessage RequestBody {
                    .opcode = ARPMessage::OPCODE_REQUEST,
                    .sender_ethernet_address = ethernet_address_,
                    .sender_ip_address = ip_address_.ipv4_numeric(),
                    .target_ethernet_address = {0, 0, 0, 0, 0, 0},
                    .target_ip_address = next_hop.ipv4_numeric()
            };
            EthernetFrame ARPRequest {
                    .header = {
                            .dst = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
                            .src = ethernet_address_,
                            .type = EthernetHeader::TYPE_ARP
                    },
                    .payload = serialize(RequestBody)
            };
            transmit(ARPRequest);
            ARPResendQueue[next_hop.ipv4_numeric()] = currentTimeStamp;
        }else{
            ARPResendQueue.erase(next_hop.ipv4_numeric());
        }
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
    transmit(dataFrame);
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if(frame.header.dst == ETHERNET_BROADCAST || frame.header.dst == ethernet_address_){
      if(frame.header.type == EthernetHeader::TYPE_ARP){
          ARPMessage ARPMessage {};
          if(!parse(ARPMessage, frame.payload)){
              return;
          }
          ARP_cache.addEntry(ARPMessage.sender_ip_address, ARPMessage.sender_ethernet_address, currentTimeStamp);
          if(ARPMessage.opcode == ARPMessage::OPCODE_REQUEST){
              if(ARPMessage.target_ip_address == ip_address_.ipv4_numeric()){
                    struct ARPMessage replyBody {
                        .opcode = ARPMessage::OPCODE_REPLY,
                        .sender_ethernet_address = ethernet_address_,
                        .sender_ip_address = ip_address_.ipv4_numeric(),
                        .target_ethernet_address = ARPMessage.sender_ethernet_address,
                        .target_ip_address = ARPMessage.sender_ip_address
                    };
                    EthernetFrame replyFrame {
                            .header = {
                                    .dst = ARPMessage.sender_ethernet_address,
                                    .src = ethernet_address_,
                                    .type = EthernetHeader::TYPE_ARP
                            },
                            .payload = serialize(replyBody)
                    };
                    transmit(replyFrame);
              }
          }else if(ARPMessage.opcode == ARPMessage::OPCODE_REPLY){
              if(sendQueue.contains(ARPMessage.sender_ip_address)){
                  const auto& dgrams = sendQueue.at(ARPMessage.sender_ip_address);
                  for(size_t i = 0; i < dgrams.size(); i++) {
                      const auto& dgram = dgrams[i];
                      EthernetFrame replyFrame{
                              .header = {
                                      .dst = ARPMessage.sender_ethernet_address,
                                      .src = ethernet_address_,
                                      .type = EthernetHeader::TYPE_IPv4
                              },
                              .payload = serialize(dgram)
                      };
                      transmit(replyFrame);
                  }
                  sendQueue.erase(ARPMessage.sender_ip_address);
              }
          }
      }else if(frame.header.type == EthernetHeader::TYPE_IPv4){
            InternetDatagram dgram;
            parse(dgram, frame.payload);
            datagrams_received_.push(dgram);
      }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
    currentTimeStamp += ms_since_last_tick;
}

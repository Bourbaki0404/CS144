#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  std::shared_ptr<TrieNode> ptr = root;
  for(int i = 0; i < prefix_length; ++i){
      bool bit = (route_prefix & (1 << (31 - i))) != 0;
      if(!ptr->children[bit]){
          ptr->children[bit] = make_shared<TrieNode>();
      }
      ptr = ptr->children[bit];
  }
  ptr->REntry = RTE{};
  ptr->REntry->interface_num = interface_num;
  if(next_hop.has_value())
    ptr->REntry->next_hop = next_hop.value();
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
    for(auto& interface : _interfaces){
        auto& SendQueue = interface->datagrams_received();
        while(!SendQueue.empty()){
            auto& dgram = SendQueue.front();
            if(dgram.header.ttl > 1){
                dgram.header.ttl--;
                dgram.header.compute_checksum();
                std::optional<RTE> RTEntry = bestMatch(dgram.header.dst);
                if(RTEntry.has_value()){
                    if(RTEntry->next_hop.has_value()){
                        Router::interface(RTEntry->interface_num)->send_datagram(dgram, RTEntry->next_hop.value());
                    }else{
                        Router::interface(RTEntry->interface_num)->send_datagram(dgram,Address::from_ipv4_numeric(dgram.header.dst));
                    }
                }
            }
            SendQueue.pop();
        }
    }
}

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

  // 首先是否<route_prefix, prefix_length>已经存在route_table中
  auto iter = router_table_.begin();
  while (iter != router_table_.end()) {
    if (iter->route_prefix == route_prefix && iter->prefix_length == prefix_length) {
      iter->next_hop = next_hop;
      iter->interface_num = interface_num;
      break;
    } else {
      iter++;
    }
  }
  if (iter == router_table_.end()) {
    router_table_.emplace_back(RouteEntry{route_prefix, prefix_length, next_hop, interface_num});
  }
}

// 该函数寻找该ip地址最适配的<next_top, interface>
int Router::match(uint32_t target_ip_address) {

  int index_matched = -1;
  for (size_t index=0;index < router_table_.size();index++) {
    if (static_cast<uint32_t>(32 - router_table_[index].prefix_length) == 32) {
      index_matched = index;
      continue;
    }
    auto mask = (0xFFFFFFFFU << static_cast<uint32_t>(32 - router_table_[index].prefix_length));
    if ((router_table_[index].route_prefix & mask) == (target_ip_address & mask)) {
      if (index_matched == -1 || router_table_[index_matched].prefix_length < router_table_[index].prefix_length) {
        index_matched = index;
      }
    }
  }
  return index_matched;
}

void Router::route() {
  for (auto &&interface : interfaces_) {
    auto dgram = interface.maybe_receive();
    if (dgram.has_value()) {
      std::cout << dgram->header.to_string() << std::endl;
      const uint32_t target_ip_address = dgram->header.dst;
      const int index = match(target_ip_address);
      if (index == -1) {
        continue;
      }

      auto next_hop = router_table_[index].next_hop;
      auto interface_num = router_table_[index].interface_num;

      // 如果当前的ttl已经是0或者当前的ttl减一之后为零，则丢弃这个数据包
      if (dgram->header.ttl == 1
           || dgram->header.ttl == 0) {
        continue;
      }

      dgram->header.ttl-=1;
      dgram->header.compute_checksum();

      if (next_hop.has_value()) {
        interfaces_[interface_num].send_datagram(dgram.value(), next_hop.value());
      } else {
        interfaces_[interface_num].send_datagram(dgram.value(), Address::from_ipv4_numeric(target_ip_address));
      }
    }
  }
}



#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address ), mappings_(),
  ready_frames_(), unready_frames_()
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
    /**
     * 寻找next_hop所对应的MAC是否已知：
     *  1. 如果Mac地址知道的话，则创建一个Ethernet frame,设置该EthernetFrame并发送
     *  2. 如果Mac地址不知道的话，则广播这个ARP查询这个IP地址所对应的MAC地址，并存储起来这个<InternetDatagram, next_hop>,
     *      一旦接受到IP地址所对应的MAC地址，则发送该网络数据报。
     * */
     EthernetFrame frame;
    if (mappings_.contains(next_hop.ipv4_numeric())) {
        auto [eAddress, time] = mappings_[next_hop.ipv4_numeric()];
        frame.header.type = EthernetHeader::TYPE_IPv4;
        frame.header.dst = eAddress;
        frame.header.src = ethernet_address_;
        frame.payload = serialize(dgram);
        ready_frames_.push_back(frame);

    } else {
        //ARP映射表中没有查询到Mac地址，所以需要发送ARP请求信息
        unready_frames_.emplace_back(dgram, next_hop);

        // 如果ARP请求从没有发送过
        if (!arp_times_.contains(next_hop.ipv4_numeric())) {
          arp_times_[next_hop.ipv4_numeric()] = timestamp_;
          // 创建ARP Message
          ARPMessage arpMessage;
          arpMessage.opcode = ARPMessage::OPCODE_REQUEST;
          arpMessage.sender_ip_address = ip_address_.ipv4_numeric();
          arpMessage.sender_ethernet_address = ethernet_address_;
          arpMessage.target_ip_address = next_hop.ipv4_numeric();
          arpMessage.target_ethernet_address = {};


          // 创建Ethernet Message
          frame.header.src = ethernet_address_;
          frame.header.dst = ETHERNET_BROADCAST;
          frame.header.type = EthernetHeader::TYPE_ARP;
          frame.payload = serialize(arpMessage);
          ready_frames_.push_back(frame);
        }

    }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
    if (frame.header.type == EthernetHeader::TYPE_IPv4) {
        // 如果该Ethernet的Header.type == TYPE_IPv4, 说明传输过来的是数据包，所以直接把数据向上传输
        InternetDatagram dgram;
        if (parse(dgram, frame.payload) && frame.header.dst == ethernet_address_) {
          return dgram;
        }

    } else if (frame.header.type == EthernetHeader::TYPE_ARP) {
        ARPMessage arpMessage;
        if (parse(arpMessage, frame.payload)) {
            auto sender_ip = arpMessage.sender_ip_address;
            auto sender_ethernet = arpMessage.sender_ethernet_address;
            mappings_[sender_ip] = make_pair(sender_ethernet, timestamp_);
            if (arp_times_.contains(sender_ip)) {
              arp_times_.erase(sender_ip);
            }
            while (!unready_frames_.empty()) {
                auto [dgram, addr] = unready_frames_.front();
                if (mappings_.contains(addr.ipv4_numeric())) {
                    send_datagram(dgram, addr);
                    unready_frames_.pop_front();
                }
            }
            if (arpMessage.opcode == ARPMessage::OPCODE_REQUEST && arpMessage.target_ip_address == ip_address_.ipv4_numeric()) {
                // 如果发送的ARP信息并且是想知道我的IP地址所对应的MAC地址
                ARPMessage message;
                message.opcode = ARPMessage::OPCODE_REPLY;
                message.sender_ip_address = ip_address_.ipv4_numeric();
                message.sender_ethernet_address = ethernet_address_;
                message.target_ip_address = arpMessage.sender_ip_address;
                message.target_ethernet_address = arpMessage.sender_ethernet_address;

                EthernetFrame reply_frame;
                reply_frame.header.type = EthernetHeader::TYPE_ARP;
                reply_frame.header.src = ethernet_address_;
                reply_frame.header.dst = arpMessage.sender_ethernet_address;
                reply_frame.payload = serialize(message);
                ready_frames_.push_back(reply_frame);
            }

        }
    }
    return {};
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  timestamp_ += ms_since_last_tick;
  auto arp_iter = arp_times_.begin();
  while (arp_iter != arp_times_.end()) {
        if (timestamp_ - arp_iter->second >= 5000) {
            arp_times_.erase(arp_iter++);
        } else {
            arp_iter++;
        }
  }
  // 刪除过期的映射
  auto map_iter = mappings_.begin();
  while (map_iter != mappings_.end()) {
        auto [eAddr, time] = map_iter->second;
        if (timestamp_ - time >= 30000) {
            mappings_.erase(map_iter++);
        } else {
            map_iter++;
        }
  }

}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
    if (ready_frames_.empty()) {
        return {};
    }
    auto frame = ready_frames_.front();
    ready_frames_.pop_front();
    return frame;
}


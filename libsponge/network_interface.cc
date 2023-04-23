#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    if (_ARP_table.count(next_hop_ip)) {
        EthernetFrame e;
        e.header().type = EthernetHeader::TYPE_IPv4;
        e.header().src = _ethernet_address;
        e.header().dst = _ARP_table[next_hop_ip].first;
        e.payload() = dgram.serialize();
        _frames_out.push(e);
    } else {
        // 注意5s内向相同的next_hop发送arp
        if (_Waiting_time.count(next_hop_ip)) {
            return;
        }

        ARPMessage arp_q;
        arp_q.opcode = ARPMessage::OPCODE_REQUEST;
        arp_q.sender_ip_address = _ip_address.ipv4_numeric();
        arp_q.sender_ethernet_address = _ethernet_address;
        arp_q.target_ip_address = next_hop_ip;
        arp_q.target_ethernet_address = {};

        EthernetFrame e;
        e.header().type =EthernetHeader::TYPE_ARP;
        e.header().src = _ethernet_address;
        e.header().dst = ETHERNET_BROADCAST;
        e.payload() = arp_q.serialize();
        _frames_out.push(e);

        _WaitingList.push_back({dgram, next_hop, 0});
        _Waiting_time[next_hop_ip] = 0;
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST) {
        return nullopt;
    }

    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        if (dgram.parse(frame.payload()) == ParseResult::NoError)
            return dgram;
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp;
        if (arp.parse(frame.payload()) == ParseResult::NoError) {
            // remember ip, mac, ttl
            _ARP_table[arp.sender_ip_address] = {arp.sender_ethernet_address, 0};

            // 如果收到了某个next_hop的mac，直接发送，更新waitinglist
            for (auto it = _WaitingList.begin(); it != _WaitingList.end(); ) {
                if (it->next_hop.ipv4_numeric() == arp.sender_ip_address) {
                    send_datagram(it->dgram, it->next_hop);   
                    _Waiting_time.erase(it->next_hop.ipv4_numeric());
                    it = _WaitingList.erase(it);    
                } else {
                    it++;
                }
            }

            if (arp.opcode == ARPMessage::OPCODE_REQUEST) {
                // send a reply
                if (arp.target_ip_address == _ip_address.ipv4_numeric()) {
                    ARPMessage arp_rp;
                    arp_rp.opcode = ARPMessage::OPCODE_REPLY;
                    arp_rp.sender_ip_address = _ip_address.ipv4_numeric();
                    arp_rp.sender_ethernet_address = _ethernet_address;
                    arp_rp.target_ip_address = arp.sender_ip_address;
                    arp_rp.target_ethernet_address = arp.sender_ethernet_address;

                    EthernetFrame e;
                    e.header().type =EthernetHeader::TYPE_ARP;
                    e.header().src = _ethernet_address;
                    e.header().dst = arp.sender_ethernet_address;
                    e.payload() = arp_rp.serialize();
                    _frames_out.push(e);
                }
            }
        }
    }

    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    for (auto it = _ARP_table.begin(); it != _ARP_table.end(); ) {
        it->second.second += ms_since_last_tick;
        if (it->second.second >= _max_arp_entry_ttl) {
            it = _ARP_table.erase(it);  //erase返回值是更新后的容器的被删除元素的后一个元素的迭代器
        } else {
            it++;
        }
    }

    for (auto it = _WaitingList.begin(); it != _WaitingList.end(); ) {
        it->waiting_time += ms_since_last_tick;
        _Waiting_time[it->next_hop.ipv4_numeric()] -= ms_since_last_tick;
        if (_ARP_table.count(it->next_hop.ipv4_numeric()) || it->waiting_time >= _max_arp_response_ttl) {
            send_datagram(it->dgram, it->next_hop);   
            _Waiting_time.erase(it->next_hop.ipv4_numeric());
            it = _WaitingList.erase(it);  // Todo: vector的erase方法效率问题
        } else {
            it++;
        }
    }
}

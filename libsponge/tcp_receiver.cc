#include "tcp_receiver.hh"
// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.
#include <iostream>

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader header = seg.header();

    if (!_isn.has_value() && !header.syn) return false;
    if (_isn.has_value() && header.syn) return false;
    if (header.fin && _reassembler.stream_out().input_ended()) return false;

    if (header.syn) {
        _isn = header.seqno;
    }
    
    uint64_t abs_ackno = _reassembler.stream_out().bytes_written() + 1;
    uint64_t window_sz = window_size();
    uint64_t abs_seqno = unwrap(header.seqno, _isn.value(), abs_ackno);
    uint64_t index = abs_seqno - 1 + header.syn;

    _reassembler.push_substring(seg.payload().copy(), index, header.fin);

    if (index + seg.payload().copy().size() == abs_ackno - 1 || index == abs_ackno - 1 + window_sz) {
        return !(seg.payload().copy().size());
    }

    return index + seg.payload().copy().size() > abs_ackno - 1 && index < abs_ackno - 1 + window_sz;
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (!_isn.has_value()) return nullopt;

    uint64_t abs_ackno = _reassembler.stream_out().bytes_written() + 1;
    if (_reassembler.stream_out().input_ended()) 
        abs_ackno++;
    
    return wrap(abs_ackno, _isn.value());
 }

size_t TCPReceiver::window_size() const { 
    return _capacity - _reassembler.stream_out().buffer_size();
 }

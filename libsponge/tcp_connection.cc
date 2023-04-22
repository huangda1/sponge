#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { 
    return _sender.stream_in().remaining_capacity(); 
}

size_t TCPConnection::bytes_in_flight() const { 
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const { 
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const { 
    return _time_since_last_segment_received;
}

void TCPConnection::_send_rst_seg() {
    // if (!_sender.segments_out().size()) 
    _sender.send_empty_segment();
    
    auto seg = _sender.segments_out().front();
    _sender.segments_out().pop();

    seg.header().rst = true;
    _sender.segments_out().push(seg);
    _send_with_ackno_and_win();
}

bool TCPConnection::_check_inbound_ended() {
    return (!unassembled_bytes() && inbound_stream().input_ended());
}

bool TCPConnection::_check_outbound_ended() {
    // todo: The outbound stream has been fully acknowledged by the remote peer.
    return (_sender.stream_in().eof() && !bytes_in_flight() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2);
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received = 0;

    if (seg.header().rst) {
        inbound_stream().set_error();
        _sender.stream_in().set_error();
        _active = false;
        return;
    }

    _receiver.segment_received(seg);
    

    if (_check_inbound_ended() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }

    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _send_with_ackno_and_win();
    }

    if (seg.length_in_sequence_space() > 0) {
        _sender.fill_window();
        bool send = _send_with_ackno_and_win();
        if (!send) {
            _sender.send_empty_segment();
            TCPSegment ack_seg = _sender.segments_out().front();
            _sender.segments_out().pop();
            ack_seg.header().ack = true;
            _sender.segments_out().push(ack_seg);
            _send_with_ackno_and_win();
        }
    }
}

bool TCPConnection::active() const { 
    return _active;
}

size_t TCPConnection::write(const string &data) {
    size_t bytes_written = _sender.stream_in().write(data);
    _sender.fill_window();
    _send_with_ackno_and_win();
    return bytes_written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;
    
    _sender.tick(ms_since_last_tick);

    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        inbound_stream().set_error();
        _sender.stream_in().set_error();
        _active = false;

        // 在发送 rst 之前，需要清空可能重新发送的数据包
        while(_sender.segments_out().size()) {
            _sender.segments_out().pop();
        }
        _send_rst_seg();
        return;
    } 
    
    // 重传
    _send_with_ackno_and_win();

    if (_check_inbound_ended() && _check_outbound_ended()) {
        if (!_linger_after_streams_finish) {
            _active = false;
        } else if (_time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
            _active = false;
        }
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    _send_with_ackno_and_win();
}

void TCPConnection::connect() {
    _sender.fill_window();
    _send_with_ackno_and_win();
}

bool TCPConnection::_send_with_ackno_and_win() {
    bool suc = false;
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = static_cast<uint16_t>(_receiver.window_size());
        }
        segments_out().push(seg);
        suc = true;
    }
    return suc;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            _sender.stream_in().set_error();
            inbound_stream().set_error();
            _active = false;
            _send_rst_seg();
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

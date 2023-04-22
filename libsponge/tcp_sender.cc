#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <iostream>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer()
    , _RTO(retx_timeout){}

uint64_t TCPSender::bytes_in_flight() const { 
    return _outstanding_bytes;
 }

void TCPSender::fill_window() {
    size_t window_size = _window_size ? _window_size: 1;

    // 考虑_outstanding_bytes的原因是防止内存中保存的outstanding_segs过多
    while (window_size > _outstanding_bytes) {
        TCPSegment seg;
        if (!_set_syn_flag) {
            _set_syn_flag = true;
            seg.header().syn = true;
        }
        
        // need to set syn、fin、seqno、 payload
        seg.header().seqno = next_seqno();
        string data = stream_in().read(min(TCPConfig::MAX_PAYLOAD_SIZE, window_size - _outstanding_bytes - seg.header().syn));

        seg.payload() = Buffer(move(data));
        if (!_set_fin_flag && stream_in().eof() && _outstanding_bytes + seg.length_in_sequence_space() < window_size) {
            _set_fin_flag = true;
            seg.header().fin = true;
        }

        if (seg.length_in_sequence_space() == 0) {
            break;
        }

        if (!_timer.is_running()) {
            _timer.start();
            _timer.init(_RTO);
        }

        segments_out().push(seg);

        _outstanding_bytes += seg.length_in_sequence_space();
        _outstanding_segs.push({_next_seqno, seg});
        _next_seqno += seg.length_in_sequence_space();

        if (seg.header().fin) {
            break;
        }
    }

}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // invoke fill_window
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    
    if (abs_ackno > _next_seqno) return false;
    if (_outstanding_segs.size() && abs_ackno < _outstanding_segs.front().first) return true;

    _RTO = _initial_retransmission_timeout;
    if (_outstanding_segs.size()) {
        if (!_timer.is_running()) _timer.start();
        _timer.init(_RTO);
    }
    _consecutive_retransmissions_num = 0;

    while (_outstanding_segs.size()) {
        auto it = _outstanding_segs.front();
        if (it.first + it.second.length_in_sequence_space() <= abs_ackno) {
            _outstanding_bytes -= it.second.length_in_sequence_space();
            _outstanding_segs.pop();
        } else {
            break;
        }
    }

    if (!_outstanding_segs.size()) {
        _timer.stop();
    }

    _window_size = window_size;
    // when new window opened up, we should consider our outstanding segments, and then decide whether to add new 
    fill_window();

    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    _timer.update_time(ms_since_last_tick);
    if (_timer.is_expired() && _outstanding_segs.size()) {
        segments_out().push(_outstanding_segs.front().second);
        if (_window_size) {
            ++_consecutive_retransmissions_num;
            _RTO *= 2;
        }
        if (!_timer.is_running()) _timer.start();
        _timer.init(_RTO);
    }
 }

unsigned int TCPSender::consecutive_retransmissions() const {
    return _consecutive_retransmissions_num;
}

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    // don't need to keep tracking this seg
    segments_out().push(seg);
}

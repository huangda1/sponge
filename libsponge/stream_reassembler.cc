#include "stream_reassembler.hh"
#include <iostream>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _index_to_str(), _output(capacity), _capacity(capacity), _first_unread_index(0), _first_unassemble_index(0), _unassemble_size(0), _eof_index(-1) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) _eof_index = index + data.size();
    if (_eof_index != -1 && _first_unassemble_index == static_cast<size_t>(_eof_index)) {
        _output.end_input();
    }

    if (index >= _first_unread_index + _capacity) return;
    if (index + data.size() <= _first_unread_index) return;

    // 只合并[_first_unread_index,_fist_acceptable_index)
    int left = max(index, _first_unread_index), right = min(index + data.size(), _first_unread_index + _capacity);
    int nl = left, nr = right;
    string s = data.substr(left - index, right - left);

    while (true) {
        auto it = _index_to_str.lower_bound(index);
        if (it == _index_to_str.end()) break;
        
        int l = it->first - it->second.size(), r = it->first;
        if (l > right) break;
        // 维护s
        if (l < nl) {
            s = it->second.substr(0, nl - l) + s;
            nl = l;
        }
        
        if (r > nr) {
            s = s + it->second.substr(nr - l);
            nr = r;
        }
        _unassemble_size -= r - l;
        _index_to_str.erase(it);
    }
    // 插入s
    _index_to_str[nr] = s;
    _unassemble_size += s.size();

    while (true) {
        auto it = _index_to_str.upper_bound(_first_unassemble_index);
        if (it != _index_to_str.end() && it->first - it->second.size() == _first_unassemble_index) {
            _first_unassemble_index = it->first;
            _unassemble_size -= it->second.size();
        } else {
            break;
        }
    }

    while (true) {
        auto it = _index_to_str.begin();
        if (it != _index_to_str.end() && it->first - it->second.size() == _first_unread_index) {
            const size_t write_bytes = _output.write(it->second);
            // str能不能被全写
            if (write_bytes == it->second.size()) {
                _first_unread_index = it->first;
                _index_to_str.erase(it);
            } else {
                _index_to_str[it->first] = it->second.substr(write_bytes);
                _first_unread_index = it->first - it->second.size();
                break;
            }
        } else {
            break;
        }
    }

    if (_eof_index != -1 && _first_unassemble_index == static_cast<size_t>(_eof_index)) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassemble_size; }

bool StreamReassembler::empty() const { return _unassemble_size == 0; }

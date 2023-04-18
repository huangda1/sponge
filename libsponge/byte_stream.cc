#include "byte_stream.hh"
#include <cassert>
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity):
    _cap(capacity),  _size(0), _bytes_written(0), _bytes_read(0), _head(0), _tail(0), _endinput(false), _data(vector<char>(capacity)) {}

size_t ByteStream::write(const string &data) {
    size_t num = std::min(_cap - _size, data.size());
    _bytes_written += num;
    for (size_t i = 0; i < num; i++) {
        _data[_tail] = data[i];
        _size++;
        _tail = (_tail + 1) % _cap;
    }
    return num;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t peek_size = min(len, _size);
    string res;
    for (size_t i = 0; i < peek_size; i++) {
        res += _data[(_head + i) % _cap];
    }
    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    // assert(len <= _size);
    size_t pop_size = min(len, _size);
    _bytes_read += pop_size;
    _size -= pop_size;
    for (size_t i = 0; i < pop_size; i++) {
        _head = (_head + 1) % _cap;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res = this->peek_output(len);
    this->pop_output(len);
    return res;
}

void ByteStream::end_input() {
    _endinput = true;
}

bool ByteStream::input_ended() const { 
    return _endinput; 
}

size_t ByteStream::buffer_size() const { 
    return _size; 
}

bool ByteStream::buffer_empty() const { 
    return _size == 0;
}

bool ByteStream::eof() const { 
    return _endinput && _size == 0;
}

size_t ByteStream::bytes_written() const { 
    return _bytes_written;
}

size_t ByteStream::bytes_read() const { 
    return _bytes_read;
}

size_t ByteStream::remaining_capacity() const { 
    return _cap - _size;
}

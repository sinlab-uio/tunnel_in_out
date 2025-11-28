#pragma once

#include <vector>

#include <stdint.h>

class UDPPacket
{
    uint32_t          _size;
    std::vector<char> _buffer;

public:
    UDPPacket( uint32_t length )
        : _size( length )
    { }

    /* Append bytes from a char vector to this UDP packet's char vector.
     * Return false if the packet is incomplete.
     * Return true if the packet is complete.
     */
    bool append( std::vector<char>& input );

    const char* get()   const { return _buffer.data(); }
    size_t      size()  const { return _size; }
    bool        ready() const { return _buffer.size() == _size; }
};


#include <iostream>

#include "udp_packet.h"

bool UDPPacket::append( std::vector<char>& input )
{
    const int isize = input.size();
    if( isize == 0 )
    {
        std::cerr << __PRETTY_FUNCTION__ << " trying to append an empty char vector" << std::endl;
        return false;
    }

    int missing = _size - _buffer.size();
    if( missing > 0 )
    {
        if( missing == isize )
        {
            _buffer.insert( _buffer.end(), input.begin(), input.end() );
            input.clear();
            return true;
        }
        else if( missing >= isize )
        {
            _buffer.insert( _buffer.end(), input.begin(), input.end() );
            input.clear();
            return false;
        }
        else
        {
            auto it = input.begin();
            _buffer.insert( _buffer.end(), it, it+missing );
            input.erase( it, it+missing );
            return true;
        }
    }
    else
    {
        std::cerr << __PRETTY_FUNCTION__ << " Programming error: trying to append bytes to a UDPPacket that is not missing any bytes." << std::endl;
        return true;
    }
}


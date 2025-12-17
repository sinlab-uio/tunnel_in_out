#include "udp_reconstructor.h"

#include <iostream>

#include <string.h> // for strerror

#include "sockaddr.h"

void UDPReconstructor::setNoBlock( UDPSocket& write_socket )
{
    write_socket.setNoBlock();
}

#define CERR if(verbose) std::cerr << __FILE__ << ":" << __LINE__

void UDPReconstructor::collect_from_tunnel( const char* buffer, int buflen )
{
    CERR << " Appending " << buflen << " bytes to reconstruction buffer" << std::endl
            << "        Bytes before appending: " << _bytes_from_tunnel.size() << std::endl;

    _bytes_from_tunnel.insert( _bytes_from_tunnel.end(), buffer, buffer+buflen );

    CERR << " Bytes after appending: " << _bytes_from_tunnel.size() << std::endl;

    while( !_bytes_from_tunnel.empty() )
    {
        CERR << " reconstruction buffer contains "
             << _bytes_from_tunnel.size() << " bytes" << std::endl;

        if( _wait_for_length ) // waiting for a length indicator from the tunnel
        {
            if( _bytes_from_tunnel.size() >= sizeof(uint32_t) )
            {
                CERR << " taking length field from reconstruction buffer" << std::endl;

                // read the length field and convert it to host byte order
                uint32_t length;
                memcpy( &length, _bytes_from_tunnel.data(), sizeof(uint32_t) );
                length = ntohl( length );

                CERR << " expecting " << length << " bytes for next UDP packet" << std::endl;

                // erase the 4 consumed bytes from the buffer
                auto it = _bytes_from_tunnel.begin();
                _bytes_from_tunnel.erase( it, it+sizeof(uint32_t) );

#if 0
                // Put an incomplete UDP packet into the packet queue
                reconstructed_packets.emplace_back( UDPPacket( length ) );
                CERR << " empty UDP packet created and inserted" << std::endl;
#endif

                _wait_for_data   = length;
                _wait_for_length = false;
            }
            else
            {
                CERR << " no length field yet, waiting" << std::endl;
                // not enough received, go back to dispatch loop
                break;
            }
        }
        else if( _bytes_from_tunnel.size() >= _wait_for_data )
        {
            CERR << " waiting for " << _wait_for_data << " bytes, have " << _bytes_from_tunnel.size() << " in bytter" << std::endl;

            UDPPacket pkt( _wait_for_data );
            bool packet_complete = pkt.append( _bytes_from_tunnel );
            if( !packet_complete )
            {
                std::cerr << __FILE__ << ":" << __LINE__ << " Programming error: created UDP packet is incomplete although we have enough data" << std::endl;
                exit( -1 );
            }

            CERR << " a packet of size " << pkt.size() << " is complete" << std::endl;
            reconstructed_packets.emplace_back( pkt );
            _wait_for_length = true;
            _wait_for_data   = 0;

            CERR << " bytes still in reconstructions buffer: " << _bytes_from_tunnel.size() << std::endl;
        }
        else if( _bytes_from_tunnel.empty() )
        {
            CERR << " received buffer on tunnel output is empty" << std::endl;
            break;
        }
        else
        {
            CERR << " waiting for " << _wait_for_data << " bytes, have only " << _bytes_from_tunnel.size() << std::endl;
            break;
        }
    }
}

void UDPReconstructor::sendLoop( UDPSocket& udp_forwarder, const SockAddr& dest )
{
    while( !write_blocked && !reconstructed_packets.empty() )
    {
        const UDPPacket& pkt( reconstructed_packets.front() );

        int retval = udp_forwarder.send( pkt.get(), pkt.size(), dest );
        if( retval >= 0 )
        {
            std::cerr << "Sent a UDP packet of size " << pkt.size() << std::endl;
            /* Passed the packet to the ::sendto function, we
             * can destroy the queue entry now.
             */
            reconstructed_packets.erase( reconstructed_packets.begin() );
        }
        else if( errno == EWOULDBLOCK )
        {
            /* Failed to send that UDP packet. Waiting for the
             * socket to report writeable again.
             */
            write_blocked = true;
            break;
        }
        else
        {
            reconstructed_packets.erase( reconstructed_packets.begin() );
        }
    }
}


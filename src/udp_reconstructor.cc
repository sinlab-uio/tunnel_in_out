#include "udp_reconstructor.h"

#include <iostream>

#include <string.h> // for strerror

#include "sockaddr.h"

void UDPReconstructor::setNoBlock( UDPSocket& write_socket )
{
    write_socket.setNoBlock();
}

void UDPReconstructor::collect_from_tunnel( const char* buffer, int buflen )
{
    if(verbose)
        std::cerr << "    Appending " << buflen << " bytes to reconstruction buffer" << std::endl
                  << "    Bytes before appending: " << bytes_from_tunnel.size() << std::endl;

    bytes_from_tunnel.insert( bytes_from_tunnel.end(), buffer, buffer+buflen );

    if(verbose)
        std::cerr << "    Bytes after appending: " << bytes_from_tunnel.size() << std::endl;

    while( !bytes_from_tunnel.empty() )
    {
        if(verbose)
            std::cerr << "    reconstruction buffer contains "
                      << bytes_from_tunnel.size() << " bytes" << std::endl;
        if( wait_for_length ) // waiting for a length indicator from the tunnel
        {
            if(verbose)
                std::cerr << "    waiting for length field" << std::endl;

            if( bytes_from_tunnel.size() >= sizeof(uint32_t) )
            {
                if(verbose)
                    std::cerr << "    taking length field from reconstruction buffer" << std::endl;

                // read the length field and convert it to host byte order
                uint32_t length;
                memcpy( &length, bytes_from_tunnel.data(), sizeof(uint32_t) );
                length = ntohl( length );

                if(verbose)
                    std::cerr << "    expecting " << length << " bytes for next UDP packet" << std::endl;

                // erase the 4 consumed bytes from the buffer
                auto it = bytes_from_tunnel.begin();
                bytes_from_tunnel.erase( it, it+sizeof(uint32_t) );

                // Put an incomplete UDP packet into the packet queue
                reconstructed_packets.emplace_back( UDPPacket( length ) );
                if(verbose) std::cerr << "    UDP packet created and inserted" << std::endl;

                wait_for_length = false;
            }
            else
            {
                // not enough received, go back to dispatch loop
                break;
            }
        }
        else if( reconstructed_packets.empty() )
        {
            std::cerr << __FILE__ << ":" << __LINE__
                      << " Programming Error: not waiting for a length field but no incomplete UDP packet in the queue, either" << std::endl;
            exit( -1 );
        }
        else
        {
            if(verbose)
                std::cerr << "    waiting for data, appending from bytes_from_tunnel.size()" << std::endl;

            bool packet_complete = reconstructed_packets.back().append( bytes_from_tunnel );
            if( packet_complete )
            {
                std::cerr << "    a packet of size " << reconstructed_packets.back().size() << " is complete" << std::endl;
                wait_for_length = true;
            }
            if(verbose)
                std::cerr << "    bytes still in reconstructions buffer: " << bytes_from_tunnel.size() << std::endl;
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


#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include <sys/select.h>
#include <unistd.h> // for close
#include <string.h> // for strerror

#include "tunnel_in_dispatch.h"
#include "sockaddr.h"
// #include "udp.h"
// #include "tcp.h"

char udp_packet_buffer[10000];
char tcp_websock_buffer[10000];
char tcp_tunnel_buffer[10000];

static const bool verbose = false;

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

class Reconstructor
{
    bool                   wait_for_length { true };
    std::vector<char>      bytes_from_tunnel;
    std::vector<UDPPacket> reconstructed_packets;
public:
    void collect_from_tunnel( const char* buffer, int buflen );

    bool empty() const { return reconstructed_packets.empty(); }

    const UDPPacket& front() const { return reconstructed_packets.front(); }

    void sendLoop( UDPSocket& udp_forwarder, const SockAddr& dest );
};

Reconstructor reconstructor;


void dispatch_loop( TCPSocket& tunnel,
                    UDPSocket& udp_forwarder,
                    std::shared_ptr<TCPSocket> webSock,
                    const SockAddr& dest_udp,
                    const SockAddr& dest_tcp )
{
    fd_set read_fds;
    int    fd_max = 0;
    bool   cont_loop = true;

    fd_set write_fds;

    std::vector<int> read_sockets;
    read_sockets.push_back( 0 ); // stdin
    read_sockets.push_back( tunnel.socket() );
    read_sockets.push_back( udp_forwarder.socket() );

    std::vector<int> write_sockets;
    write_sockets.push_back( udp_forwarder.socket() );

    while( cont_loop )
    {
        FD_ZERO( &read_fds );
        FD_ZERO( &write_fds );
        std::cerr << "    call select with read fds ";
        for( auto it : read_sockets )
        {
            std::cerr << it << " ";
            FD_SET( it, &read_fds );
            fd_max = std::max( fd_max, it );
        }
        std::cerr << ", write fds ";
        for( auto it : write_sockets )
        {
            std::cerr << it << " ";
            FD_SET( it, &write_fds );
            fd_max = std::max( fd_max, it );
        }
        std::cerr << std::endl;

        int retval = ::select( fd_max+1, &read_fds, &write_fds, nullptr, nullptr );

        if( FD_ISSET( 0, &read_fds ) )
        {
std::cerr << __LINE__ << std::endl;
            int c = getchar( );
            if( c == 'q' || c == 'Q' )
            {
                std::cerr << "= Q pressed by user. Quitting." << std::endl;
                cont_loop = false;
            }
        }

        if( FD_ISSET( tunnel.socket(), &read_fds ) )
        {
std::cerr << __LINE__ << std::endl;
            int retval = tunnel.recv( tcp_tunnel_buffer, 10000 );
            if( retval < 0 )
            {
                std::cerr << "Error in TCP tunnel, socket " << tunnel.socket() << ". "
                          << strerror(errno) << std::endl;
                cont_loop = false;
            }
            else if( retval == 0 )
            {
                std::cerr << "= Socket " << tunnel.socket() << " closed. Quitting." << std::endl;
                cont_loop = false;
            }
            else
            {
                reconstructor.collect_from_tunnel( tcp_tunnel_buffer, retval );
            }
        }

        if( FD_ISSET( udp_forwarder.socket(), &read_fds ) )
        {
std::cerr << __LINE__ << std::endl;
            udp_forwarder.recv( udp_packet_buffer, 10000 );
        }

        if( webSock && FD_ISSET( webSock->socket(), &read_fds ) )
        {
std::cerr << __LINE__ << std::endl;
            webSock->recv( tcp_websock_buffer, 10000 );
        }

        if( FD_ISSET( udp_forwarder.socket(), &write_fds ) )
        {
std::cerr << __LINE__ << std::endl;
            reconstructor.sendLoop( udp_forwarder, dest_udp );
        }
    }
}

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

void Reconstructor::collect_from_tunnel( const char* buffer, int buflen )
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
            std::cerr << __PRETTY_FUNCTION__ << " Programming Error: not waiting for a length field but not incomplete UDP packet in the queue, either" << std::endl;
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

void Reconstructor::sendLoop( UDPSocket& udp_forwarder, const SockAddr& dest )
{
    while( !reconstructed_packets.empty() )
    {
        const UDPPacket& pkt( reconstructed_packets.front() );

        bool success = udp_forwarder.send( pkt.get(), pkt.size(), dest );
        if( success )
        {
            std::cerr << "Sent a UDP packet of size " << pkt.size() << std::endl;
            /* Passed the packet to the ::sendto function, we
             * can destroy the queue entry now.
             */
            reconstructed_packets.erase( reconstructed_packets.begin() );
        }
        else
        {
            /* Failed to send that UDP packet. Waiting for the
             * socket to report writeable again.
             */
            break;
        }
    }
}


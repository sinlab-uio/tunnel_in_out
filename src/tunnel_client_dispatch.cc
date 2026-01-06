#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include <sys/select.h>
#include <unistd.h> // for close
#include <string.h> // for strerror

#include "tunnel_client_dispatch.h"
#include "sockaddr.h"
#include "udp_packet.h"
#include "udp_reconstructor.h"
// #include "tcp.h"

static const size_t max_buffer_size = 100000;

char udp_packet_buffer[max_buffer_size];
char tcp_websock_buffer[max_buffer_size];
char tcp_tunnel_buffer[max_buffer_size];

static const bool verbose = false;

UDPReconstructor reconstructor;

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

    while( cont_loop )
    {
        std::vector<int> write_sockets;
        if( reconstructor.isWriteBlocked() )
            write_sockets.push_back( udp_forwarder.socket() );

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
            int retval = tunnel.recv( tcp_tunnel_buffer, max_buffer_size );
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
                reconstructor.sendLoop( udp_forwarder, dest_udp );
            }
        }

        if( FD_ISSET( udp_forwarder.socket(), &read_fds ) )
        {
std::cerr << __LINE__ << std::endl;
            udp_forwarder.recv( udp_packet_buffer, max_buffer_size );
        }

        if( webSock && FD_ISSET( webSock->socket(), &read_fds ) )
        {
std::cerr << __LINE__ << std::endl;
            webSock->recv( tcp_websock_buffer, max_buffer_size );
        }

        if( FD_ISSET( udp_forwarder.socket(), &write_fds ) )
        {
std::cerr << __LINE__ << std::endl;
            reconstructor.unblockWrite( );
            reconstructor.sendLoop( udp_forwarder, dest_udp );
        }
    }
}


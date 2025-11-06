#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include <sys/select.h>
#include <unistd.h> // for close

#include "tunnel_in_dispatch.h"
#include "sockaddr.h"
// #include "udp.h"
// #include "tcp.h"

char udp_packet_buffer[10000];
char tcp_websock_buffer[10000];
char tcp_tunnel_buffer[10000];

void dispatch_loop( TCPSocket& tunnel_listener,
                    UDPSocket& outside_udp,
                    TCPSocket& outside_tcp_listener,
                    std::shared_ptr<TCPSocket> tunnel,
                    std::shared_ptr<TCPSocket> webSock )
{
    fd_set fds;
    int    fd_max = 0;
    bool   cont_loop = true;

    std::vector<int> sockets;
    sockets.push_back( 0 ); // stdin
    sockets.push_back( tunnel_listener.socket() );
    sockets.push_back( outside_udp.socket() );
    sockets.push_back( outside_tcp_listener.socket() );

    while( cont_loop )
    {
        FD_ZERO( &fds );
        std::cerr << "    call select with sockets ";
        for( auto it : sockets )
        {
            std::cerr << it << " ";
            FD_SET( it, &fds );
            fd_max = std::max( fd_max, it );
        }
        std::cerr << std::endl;

        fd_max += 1;

        int retval = ::select( fd_max, &fds, nullptr, nullptr, nullptr );

        if( retval < 0 )
        {
            perror( "Select failed. ");
        }

        if( FD_ISSET( 0, &fds ) )
        {
            int c = getchar( );
            if( c == 'q' || c == 'Q' )
            {
                std::cout << "= Q pressed by user. Quitting." << std::endl
                          << "= Note: TCP tunnel port will be unavailable for up to a minute" << std::endl
                          << "=       if TunnelOut was currently connected." << std::endl;
                cont_loop = false;
            }
        }

        if( FD_ISSET( tunnel_listener.socket(), &fds ) )
        {
            /* Create a new TCP socket from the connect request. Run connect
             * in the constructor to consume the data. */
            std::shared_ptr<TCPSocket> tcp_conn( new TCPSocket( tunnel_listener ) );
            if( tcp_conn->valid() )
            {
                tunnel.swap( tcp_conn );
                sockets.push_back( tunnel->socket() );
                std::cout << "= Connection from TunnelIn established on port " << tunnel->getPort()
                          << ", socket " << tunnel->socket() << std::endl;
            }
            else
            {
                std::cerr << "    Activity on tunnel listener socket, but accept failed" << std::endl;
            }
        }

        if( FD_ISSET( outside_tcp_listener.socket(), &fds ) )
        {
            /* Create a new TCP socket from the connect request. Run connect
             * in the constructor to consume the data. */
            std::shared_ptr<TCPSocket> tcp_conn( new TCPSocket( outside_tcp_listener ) );
            if( tcp_conn->valid() )
            {
                webSock.swap( tcp_conn );
                sockets.push_back( webSock->socket() );
            }
        }

        if( FD_ISSET( outside_udp.socket(), &fds ) )
        {
            // std::cerr << "Activity on UDP socket " << outside_udp.socket() << std::endl;
            retval = outside_udp.recv( udp_packet_buffer, 10000 );
            if( retval < 0 )
            {
                std::cerr << "Read from outside UDP socket failed. " << strerror(errno) << std::endl;
            }
            else if( retval == 0 )
            {
                std::cerr << "recv on outside UDP socket " << outside_udp.socket() 
                          << " returned " << retval << std::endl;
            }
            else
            {
                std::cout << "= Received a packet (" << retval << " bytes) on outside UDP port "
                          << outside_udp.getPort() << ", socket " << outside_udp.socket() << std::endl;
                if( tunnel )
                {
                    uint32_t pkt_len = htonl( retval );
                    tunnel->send( &pkt_len, sizeof(uint32_t) ); // size is always 4
                    tunnel->send( udp_packet_buffer, retval );
                }
                else
                {
                    std::cerr << "    tunnel to TunnelIn isn't established. Drop UDP packets." << std::endl;
                }
            }
        }

        if( tunnel && FD_ISSET( tunnel->socket(), &fds ) )
        {
            int retval = tunnel->recv( tcp_tunnel_buffer, 10000 );
            if( retval == 0 )
            {
                std::cerr << "Reading from TCP tunnel returned 0. Broken connection? Quitting." << std::endl;
                cont_loop = false;
            }
        }

        if( webSock && FD_ISSET( webSock->socket(), &fds ) )
        {
            webSock->recv( tcp_websock_buffer, 10000 );
        }
    }
}


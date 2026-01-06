#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include <sys/select.h>
#include <unistd.h> // for close
#include <string.h> // for strerror
#include <errno.h>

#include "tunnel_in_dispatch.h"
#include "sockaddr.h"

#define CERR std::cerr << __FILE__ << ":" << __LINE__

static const size_t max_buffer_size = 100000;
static const size_t max_udp_packet_size = 65536; // Standard max UDP packet size

// Use smaller, stack-based buffers or heap allocation per connection
static char udp_packet_buffer[max_udp_packet_size];
static char tcp_websock_buffer[max_buffer_size];
static char tcp_tunnel_buffer[max_buffer_size];

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
        fd_max = 0;
        
        std::cerr << "    call select with sockets ";
        for( auto it : sockets )
        {
            std::cerr << it << " ";
            FD_SET( it, &fds );
            fd_max = std::max( fd_max, it );
        }
        std::cerr << std::endl;

        int retval = ::select( fd_max + 1, &fds, nullptr, nullptr, nullptr );

        if( retval < 0 )
        {
            if (errno == EINTR)
            {
                // Interrupted by signal, retry
                continue;
            }
            perror( "Select failed. ");
            break;
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
                // Close old tunnel if exists
                if (tunnel && tunnel->valid())
                {
                    std::cout << "= Replacing existing tunnel connection" << std::endl;
                    // Remove old socket from list
                    auto it = std::find(sockets.begin(), sockets.end(), tunnel->socket());
                    if (it != sockets.end())
                    {
                        sockets.erase(it);
                    }
                }
                
                tunnel.swap( tcp_conn );
                sockets.push_back( tunnel->socket() );
                std::cout << "= Connection from TunnelOut established on port " << tunnel->getPort()
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
                // Close old webSock if exists
                if (webSock && webSock->valid())
                {
                    std::cout << "= Replacing existing web socket connection" << std::endl;
                    // Remove old socket from list
                    auto it = std::find(sockets.begin(), sockets.end(), webSock->socket());
                    if (it != sockets.end())
                    {
                        sockets.erase(it);
                    }
                }
                
                webSock.swap( tcp_conn );
                sockets.push_back( webSock->socket() );
                std::cout << "= Outside TCP connection established on socket " 
                          << webSock->socket() << std::endl;
            }
        }

        if( FD_ISSET( outside_udp.socket(), &fds ) )
        {
            // std::cerr << "Activity on UDP socket " << outside_udp.socket() << std::endl;
            retval = outside_udp.recv( udp_packet_buffer, max_udp_packet_size );
            if( retval < 0 )
            {
                CERR << " Read from outside UDP socket failed. " << strerror(errno) << std::endl;
            }
            else if( retval == 0 )
            {
                CERR << " recv on outside UDP socket " << outside_udp.socket() 
                     << " returned " << retval << std::endl;
            }
            else
            {
                CERR << " Received a packet (" << retval << " bytes) on outside UDP port "
                     << outside_udp.getPort() << ", socket " << outside_udp.socket() << std::endl;
                if( tunnel && tunnel->valid() )
                {
                    // Send length header in network byte order
                    uint32_t pkt_len = htonl( retval );
                    CERR << " Sending packet len " << retval << " on TCP" << std::endl;
                    
                    int sent = tunnel->send( &pkt_len, sizeof(uint32_t) );
                    if (sent != sizeof(uint32_t))
                    {
                        CERR << " Failed to send length header on tunnel. Connection broken?" << std::endl;
                        // Remove tunnel from socket list and mark as invalid
                        auto it = std::find(sockets.begin(), sockets.end(), tunnel->socket());
                        if (it != sockets.end())
                        {
                            sockets.erase(it);
                        }
                        tunnel.reset();
                        continue;
                    }
                    
                    CERR << " Sending " << retval << " bytes on TCP" << std::endl;
                    sent = tunnel->send( udp_packet_buffer, retval );
                    if (sent != retval)
                    {
                        CERR << " Failed to send UDP data on tunnel. Connection broken?" << std::endl;
                        // Remove tunnel from socket list and mark as invalid
                        auto it = std::find(sockets.begin(), sockets.end(), tunnel->socket());
                        if (it != sockets.end())
                        {
                            sockets.erase(it);
                        }
                        tunnel.reset();
                    }
                }
                else
                {
                    CERR << " Tunnel to TunnelOut isn't established. Drop UDP packets." << std::endl;
                }
            }
        }

        if( tunnel && FD_ISSET( tunnel->socket(), &fds ) )
        {
            int retval = tunnel->recv( tcp_tunnel_buffer, max_buffer_size );
            if( retval == 0 )
            {
                CERR << " Reading from TCP tunnel returned 0. Connection closed by peer." << std::endl;
                // Remove from socket list
                auto it = std::find(sockets.begin(), sockets.end(), tunnel->socket());
                if (it != sockets.end())
                {
                    sockets.erase(it);
                }
                tunnel.reset();
                std::cout << "= Tunnel connection closed. Waiting for new connection..." << std::endl;
            }
            else if (retval < 0)
            {
                CERR << " Error reading from tunnel: " << strerror(errno) << std::endl;
                // Remove from socket list
                auto it = std::find(sockets.begin(), sockets.end(), tunnel->socket());
                if (it != sockets.end())
                {
                    sockets.erase(it);
                }
                tunnel.reset();
            }
            else
            {
                // Data received on tunnel from TunnelOut - could be responses
                CERR << " Received " << retval << " bytes on tunnel (currently ignored)" << std::endl;
            }
        }

        if( webSock && FD_ISSET( webSock->socket(), &fds ) )
        {
            int retval = webSock->recv( tcp_websock_buffer, max_buffer_size );
            if (retval <= 0)
            {
                CERR << " Web socket closed or error" << std::endl;
                // Remove from socket list
                auto it = std::find(sockets.begin(), sockets.end(), webSock->socket());
                if (it != sockets.end())
                {
                    sockets.erase(it);
                }
                webSock.reset();
            }
            else
            {
                // TODO: Handle web socket data
                CERR << " Received " << retval << " bytes on web socket (currently ignored)" << std::endl;
            }
        }
    }
}


#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include <sys/select.h>
#include <unistd.h> // for close
#include <string.h> // for strerror
#include <errno.h>

#include "tunnel_server_dispatch.h"
#include "tunnel_protocol.h"
#include "sockaddr.h"

#define CERR std::cerr << __FILE__ << ":" << __LINE__

static const size_t max_buffer_size = 100000;
static const size_t max_udp_packet_size = 65536; // Standard max UDP packet size

// Buffers
static char udp_packet_buffer[max_udp_packet_size];
static char tcp_websock_buffer[max_buffer_size];
static char tcp_tunnel_buffer[max_buffer_size];

// Helper function to send a tunnel message
// Returns true on success, false on failure
bool sendTunnelMessage(TCPSocket& tunnel, 
                       uint32_t conn_id,
                       TunnelMessageType type,
                       const char* payload,
                       uint16_t payload_len)
{
    // Validate payload length
    if (payload_len > TunnelProtocol::MAX_PAYLOAD_SIZE)
    {
        std::cerr << "ERROR: Payload too large: " << payload_len << std::endl;
        return false;
    }
    
    // Create header
    TunnelMessageHeader header;
    TunnelProtocol::createHeader(header, conn_id, payload_len, type);
    
    // Send header
    int sent = tunnel.send(&header, TunnelProtocol::HEADER_SIZE);
    if (sent != TunnelProtocol::HEADER_SIZE)
    {
        CERR << " Failed to send message header" << std::endl;
        return false;
    }
    
    // Send payload (if any)
    if (payload_len > 0)
    {
        sent = tunnel.send(payload, payload_len);
        if (sent != payload_len)
        {
            CERR << " Failed to send message payload" << std::endl;
            return false;
        }
    }
    
    return true;
}

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
                          << "=       if TunnelClient was currently connected." << std::endl;
                cont_loop = false;
            }
        }

        if( FD_ISSET( tunnel_listener.socket(), &fds ) )
        {
            /* Create a new TCP socket from the connect request. */
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
                std::cout << "= Connection from TunnelClient established on port " << tunnel->getPort()
                          << ", socket " << tunnel->socket() << std::endl;
            }
            else
            {
                std::cerr << "    Activity on tunnel listener socket, but accept failed" << std::endl;
            }
        }

        if( FD_ISSET( outside_tcp_listener.socket(), &fds ) )
        {
            /* Accept new TCP connection from outside */
            std::shared_ptr<TCPSocket> tcp_conn( new TCPSocket( outside_tcp_listener ) );
            if( tcp_conn->valid() )
            {
                // For now, we only support one outside TCP connection
                // In future: multiple connections with conn_id management
                if (webSock && webSock->valid())
                {
                    std::cout << "= Replacing existing outside TCP connection" << std::endl;
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
                
                // TODO: Send TCP_OPEN message through tunnel when TCP support is implemented
            }
        }

        if( FD_ISSET( outside_udp.socket(), &fds ) )
        {
            // Receive UDP packet from outside
            retval = outside_udp.recv( udp_packet_buffer, max_udp_packet_size );
            if( retval < 0 )
            {
                CERR << " Read from outside UDP socket failed. " << strerror(errno) << std::endl;
            }
            else if( retval == 0 )
            {
                CERR << " recv on outside UDP socket returned 0" << std::endl;
            }
            else
            {
                CERR << " Received UDP packet (" << retval << " bytes)" << std::endl;
                
                if( tunnel && tunnel->valid() )
                {
                    // Send UDP packet through tunnel using new protocol
                    // conn_id = 0 for UDP (not using connection multiplexing yet)
                    bool success = sendTunnelMessage(*tunnel, 
                                                     0,  // conn_id = 0 for UDP
                                                     TunnelMessageType::UDP_PACKET,
                                                     udp_packet_buffer,
                                                     retval);
                    
                    if (!success)
                    {
                        CERR << " Failed to send UDP packet through tunnel. Connection broken?" << std::endl;
                        // Remove tunnel from socket list
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
                    CERR << " Tunnel to TunnelClient isn't established. Drop UDP packets." << std::endl;
                }
            }
        }

        if( tunnel && FD_ISSET( tunnel->socket(), &fds ) )
        {
            int retval = tunnel->recv( tcp_tunnel_buffer, max_buffer_size );
            if( retval == 0 )
            {
                CERR << " TCP tunnel closed by peer." << std::endl;
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
                // Data received on tunnel from TunnelClient
                // In future implementation, this will be parsed and handled
                // For now, just log it
                CERR << " Received " << retval << " bytes on tunnel" << std::endl;
                CERR << " (Message parsing not yet implemented - this would be responses from client)" << std::endl;
            }
        }

        if( webSock && FD_ISSET( webSock->socket(), &fds ) )
        {
            int retval = webSock->recv( tcp_websock_buffer, max_buffer_size );
            if (retval <= 0)
            {
                CERR << " Outside TCP socket closed" << std::endl;
                // Remove from socket list
                auto it = std::find(sockets.begin(), sockets.end(), webSock->socket());
                if (it != sockets.end())
                {
                    sockets.erase(it);
                }
                webSock.reset();
                
                // TODO: Send TCP_CLOSE message through tunnel when TCP support is implemented
            }
            else
            {
                // Data received on outside TCP connection
                CERR << " Received " << retval << " bytes on outside TCP socket" << std::endl;
                
                // TODO: Send TCP_DATA message through tunnel when TCP support is implemented
            }
        }
    }
}


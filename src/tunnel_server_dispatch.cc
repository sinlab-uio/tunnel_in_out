#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <sstream>

#include <sys/select.h>
#include <unistd.h> // for close
#include <string.h> // for strerror
#include <errno.h>

#include "tunnel_server_dispatch.h"
#include "tunnel_protocol.h"
#include "tunnel_message_reconstructor.h"
#include "sockaddr.h"
#include "verbose.h"

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
        LOG_ERROR << "Payload too large: " << payload_len << std::endl;
        return false;
    }
    
    // Create header
    TunnelMessageHeader header;
    TunnelProtocol::createHeader(header, conn_id, payload_len, type);
    
    // Send header
    int sent = tunnel.send(&header, TunnelProtocol::HEADER_SIZE);
    if (sent != TunnelProtocol::HEADER_SIZE)
    {
        LOG_ERROR << "Failed to send message header" << std::endl;
        return false;
    }
    
    // Send payload (if any)
    if (payload_len > 0)
    {
        sent = tunnel.send(payload, payload_len);
        if (sent != payload_len)
        {
            LOG_ERROR << "Failed to send message payload" << std::endl;
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

    // Track the last sender address for UDP responses
    SockAddr last_udp_sender;
    bool has_udp_sender = false;
    
    // Message reconstructor for parsing messages from TunnelClient
    TunnelMessageReconstructor reconstructor;

    while( cont_loop )
    {
        FD_ZERO( &fds );
        fd_max = 0;
        
        std::ostringstream ostr;
        ostr << "call select with sockets ";
        for( auto it : sockets )
        {
            ostr << it << " ";
            FD_SET( it, &fds );
            fd_max = std::max( fd_max, it );
        }
        LOG_DEBUG << ostr.str() << std::endl;

        int retval = ::select( fd_max + 1, &fds, nullptr, nullptr, nullptr );

        if( retval < 0 )
        {
            if (errno == EINTR)
            {
                // Interrupted by signal, retry
                continue;
            }
            LOG_ERROR << "Select faliled: " << strerror(errno) << std::endl;
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
                    LOG_INFO << "Replacing existing tunnel connection" << std::endl;
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
                LOG_WARN << "Activity on tunnel listener socket, but accept failed" << std::endl;
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
            // Receive UDP packet from outside - could be initial request OR response
            retval = outside_udp.recv( udp_packet_buffer, max_udp_packet_size, last_udp_sender );
            if( retval < 0 )
            {
                LOG_WARN << "Read from outside UDP socket failed. " << strerror(errno) << std::endl;
            }
            else if( retval == 0 )
            {
                LOG_DEBUG << "recv on outside UDP socket returned 0" << std::endl;
            }
            else
            {
                LOG_DEBUG << " Received UDP packet (" << retval << " bytes) from " 
                          << last_udp_sender.getAddress() << ":" << last_udp_sender.getPort() << std::endl;
                
                // Remember this sender for future responses
                has_udp_sender = true;
                
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
                        LOG_WARN << " Failed to send UDP packet through tunnel. Connection broken?" << std::endl;
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
                    LOG_INFO << "Tunnel to TunnelClient isn't established. Drop UDP packets." << std::endl;
                }
            }
        }

        if( tunnel && FD_ISSET( tunnel->socket(), &fds ) )
        {
            int retval = tunnel->recv( tcp_tunnel_buffer, max_buffer_size );
            if( retval == 0 )
            {
                LOG_INFO << "TCP tunnel closed by peer." << std::endl;
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
                LOG_WARN << "Error reading from tunnel: " << strerror(errno) << std::endl;
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
                LOG_DEBUG << "Received " << retval << " bytes on tunnel" << std::endl;
                
                // Feed received bytes to reconstructor
                reconstructor.collect_from_tunnel( tcp_tunnel_buffer, retval );
                
                // Process all complete messages
                while (reconstructor.hasMessages())
                {
                    TunnelMessage& msg = reconstructor.frontMessage();
                    
                    LOG_DEBUG << "Processing message from TunnelClient: type=" 
                              << TunnelProtocol::messageTypeToString(msg.type)
                              << " conn_id=" << msg.conn_id
                              << " payload_size=" << msg.payload.size() << std::endl;
                    
                    switch (msg.type)
                    {
                        case TunnelMessageType::UDP_PACKET:
                        {
                            // This is a response UDP packet from inside the firewall
                            // Forward it back to the last sender
                            if (has_udp_sender)
                            {
                                if (msg.payload.size() > 0)
                                {
                                    int sent = outside_udp.send(msg.payload.data(), 
                                                               msg.payload.size(), 
                                                               last_udp_sender);
                                    if (sent >= 0)
                                    {
                                        LOG_DEBUG << "Forwarded UDP response (" << msg.payload.size() 
                                                  << " bytes) back to " 
                                                  << last_udp_sender.getAddress() << ":" 
                                                  << last_udp_sender.getPort() << std::endl;
                                    }
                                    else
                                    {
                                        LOG_WARN << "Error forwarding UDP response: " 
                                                 << strerror(errno) << std::endl;
                                    }
                                }
                            }
                            else
                            {
                                LOG_WARN << "Received UDP response but no sender address known (no request received yet)" 
                                         << std::endl;
                            }
                            break;
                        }
                        
                        case TunnelMessageType::TCP_OPEN:
                        case TunnelMessageType::TCP_DATA:
                        case TunnelMessageType::TCP_CLOSE:
                        {
                            LOG_INFO << "TODO: Handle TCP message type " 
                                     << TunnelProtocol::messageTypeToString(msg.type) << std::endl;
                            break;
                        }
                        
                        default:
                            LOG_ERROR << "Unknown message type: " 
                                      << static_cast<int>(msg.type) << std::endl;
                            break;
                    }
                    
                    // Remove processed message
                    reconstructor.popMessage();
                }
            }
        }

        if( webSock && FD_ISSET( webSock->socket(), &fds ) )
        {
            int retval = webSock->recv( tcp_websock_buffer, max_buffer_size );
            if (retval <= 0)
            {
                LOG_WARN << "Outside TCP socket closed" << std::endl;
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
                LOG_WARN << "Received " << retval << " bytes on outside TCP socket" << std::endl;
                
                // TODO: Send TCP_DATA message through tunnel when TCP support is implemented
            }
        }
    }
}


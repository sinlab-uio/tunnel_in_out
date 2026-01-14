#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <sstream>

#include <sys/select.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "tunnel_server_dispatch.h"
#include "tunnel_protocol.h"
#include "tunnel_send_message.h"
#include "tunnel_message_reconstructor.h"
#include "tcp_connection_manager.h"
#include "sockaddr.h"
#include "verbose.h"

static const size_t max_buffer_size = 100000;
static const size_t max_udp_packet_size = 65536;
static const size_t max_tcp_data_size = 16384;  // 16KB per TCP read

// Buffers
static char udp_packet_buffer[max_udp_packet_size];
static char tcp_data_buffer[max_tcp_data_size];
static char tcp_tunnel_buffer[max_buffer_size];

void dispatch_loop( TCPSocket& tunnel_listener,
                    UDPSocket& outside_udp,
                    TCPSocket& outside_tcp_listener )
{
    fd_set fds;
    int    fd_max = 0;
    bool   cont_loop = true;

    // There can only be one open tunnel at any time.
    std::unique_ptr<TCPSocket> tunnel;

    // std::shared_ptr<TCPSocket> webSock )

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
    
    // TCP connection manager for multiplexing TCP connections
    // STATIC - preserved across tunnel reconnections
    static TCPConnectionManager tcp_connections;

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
        
        // Add all TCP connection sockets
        auto tcp_sockets = tcp_connections.getAllSockets();
        for (int sock : tcp_sockets)
        {
            ostr << sock << " ";
            FD_SET( sock, &fds );
            fd_max = std::max( fd_max, sock );
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
            LOG_ERROR << "Select failed: " << strerror(errno) << std::endl;
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
            std::unique_ptr<TCPSocket> tcp_conn( new TCPSocket( tunnel_listener, true ) );
            if( tcp_conn->valid() )
            {
                // Close old tunnel if exists
                if( tunnel && tunnel->valid() )
                {
                    LOG_INFO << "Replacing existing tunnel connection" << std::endl;
                    // Remove old socket from list
                    auto it = std::find(sockets.begin(), sockets.end(), tunnel->socket());
                    if (it != sockets.end())
                    {
                        sockets.erase(it);
                    }
                }
                
                tunnel = std::move( tcp_conn );
                sockets.push_back( tunnel->socket() );
                
                // Log preserved TCP connections after tunnel reconnect
                if (tcp_connections.connectionCount() > 0)
                {
                    LOG_INFO << "Tunnel reconnected with " << tcp_connections.connectionCount() 
                             << " preserved outside TCP connections" << std::endl;
                }
                
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
            // New TCP connection from outside
            std::unique_ptr<TCPSocket> tcp_conn( new TCPSocket( outside_tcp_listener, true ) );
            if( tcp_conn->valid() )
            {
                // Set non-blocking to avoid delaying UDP
                tcp_conn->setNoBlock();
                
                if (tunnel && tunnel->valid())
                {
                    // Allocate connection ID
                    uint32_t conn_id = tcp_connections.allocateConnId();
                    
                    LOG_INFO << "Outside TCP connection accepted on socket " 
                              << tcp_conn->socket() << ", assigned conn_id=" << conn_id << std::endl;
                    
                    // Add to connection manager
                    tcp_connections.addConnection(conn_id, tcp_conn);
                    
                    // Send TCP_OPEN message through tunnel
                    bool success = sendTunnelMessage(tunnel, 
                                                     conn_id,
                                                     TunnelMessageType::TCP_OPEN,
                                                     nullptr,
                                                     0);
                    
                    if (success)
                    {
                        LOG_DEBUG << "Sent TCP_OPEN for conn_id=" << conn_id << std::endl;
                    }
                    else
                    {
                        LOG_ERROR << "Failed to send TCP_OPEN for conn_id=" << conn_id << std::endl;
                        tcp_connections.removeConnection(conn_id);
                    }
                }
                else
                {
                    LOG_WARN << "Outside TCP connection received but tunnel not established. Rejecting." << std::endl;
                    // tcp_conn will be automatically closed when shared_ptr goes out of scope
                }
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
                LOG_DEBUG << "Received UDP packet (" << retval << " bytes) from " 
                          << last_udp_sender.getAddress() << ":" << last_udp_sender.getPort() << std::endl;
                
                // Remember this sender for future responses
                has_udp_sender = true;
                
                if( tunnel && tunnel->valid() )
                {
                    // Send UDP packet through tunnel using new protocol
                    // conn_id = 0 for UDP (not using connection multiplexing yet)
                    bool success = sendTunnelMessage(tunnel, 
                                                     0,  // conn_id = 0 for UDP
                                                     TunnelMessageType::UDP_PACKET,
                                                     udp_packet_buffer,
                                                     retval);
                    
                    if (!success)
                    {
                        LOG_WARN << "Failed to send UDP packet through tunnel. Connection broken?" << std::endl;
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

        // Check all TCP connections from outside for data
        for (uint32_t conn_id : tcp_connections.getAllConnIds())
        {
            auto* conn = tcp_connections.getConnection(conn_id);
            if (!conn || !conn->socket || !conn->valid)
                continue;
                
            if (FD_ISSET(conn->socket->socket(), &fds))
            {
                int bytes = conn->socket->recv(tcp_data_buffer, max_tcp_data_size);
                
                if (bytes == 0)
                {
                    // Connection closed by peer
                    LOG_INFO << "Outside TCP connection closed by peer, conn_id=" << conn_id << std::endl;
                    
                    if (tunnel && tunnel->valid())
                    {
                        sendTunnelMessage(tunnel, conn_id, TunnelMessageType::TCP_CLOSE, nullptr, 0);
                    }
                    
                    tcp_connections.removeConnection(conn_id);
                }
                else if (bytes < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        // Non-blocking socket, no data available
                        continue;
                    }
                    
                    LOG_WARN << "Error reading from outside TCP conn_id=" << conn_id 
                             << ": " << strerror(errno) << std::endl;
                    
                    if (tunnel && tunnel->valid())
                    {
                        sendTunnelMessage(tunnel, conn_id, TunnelMessageType::TCP_CLOSE, nullptr, 0);
                    }
                    
                    tcp_connections.removeConnection(conn_id);
                }
                else
                {
                    // Data received
                    LOG_DEBUG << "Received " << bytes << " bytes from outside TCP conn_id=" << conn_id << std::endl;
                    
                    if (tunnel && tunnel->valid())
                    {
                        bool success = sendTunnelMessage(tunnel, 
                                                         conn_id,
                                                         TunnelMessageType::TCP_DATA,
                                                         tcp_data_buffer,
                                                         bytes);
                        
                        if (!success)
                        {
                            LOG_ERROR << "Failed to send TCP_DATA for conn_id=" << conn_id << std::endl;
                            tcp_connections.removeConnection(conn_id);
                        }
                    }
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
                                        LOG_ERROR << "Error forwarding UDP response: " 
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
                        
                        case TunnelMessageType::TCP_DATA:
                        {
                            auto* conn = tcp_connections.getConnection(msg.conn_id);
                            if (conn && conn->socket && conn->valid)
                            {
                                int sent = conn->socket->send(msg.payload.data(), msg.payload.size());
                                if (sent < 0)
                                {
                                    LOG_WARN << "Failed to send TCP data to conn_id=" 
                                             << msg.conn_id << std::endl;
                                    tcp_connections.removeConnection(msg.conn_id);
                                    sendTunnelMessage(tunnel, msg.conn_id, 
                                                     TunnelMessageType::TCP_CLOSE, nullptr, 0);
                                }
                                else
                                {
                                    LOG_DEBUG << "Forwarded " << sent 
                                              << " bytes to outside TCP conn_id=" << msg.conn_id << std::endl;
                                }
                            }
                            else
                            {
                                LOG_WARN << "Received TCP_DATA for unknown conn_id=" << msg.conn_id << std::endl;
                            }
                            break;
                        }
                        
                        case TunnelMessageType::TCP_CLOSE:
                        {
                            LOG_INFO << "Received TCP_CLOSE for conn_id=" << msg.conn_id << std::endl;
                            tcp_connections.removeConnection(msg.conn_id);
                            break;
                        }
                        
                        case TunnelMessageType::TCP_OPEN:
                        {
                            LOG_WARN << "Unexpected TCP_OPEN from client for conn_id=" << msg.conn_id << std::endl;
                            break;
                        }
                        
                        default:
                            LOG_ERROR << "Unknown message type: " << static_cast<int>(msg.type) << std::endl;
                            break;
                    }
                    
                    // Remove processed message
                    reconstructor.popMessage();
                }
            }
        }
    }
}


#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include <sys/select.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "tunnel_client_dispatch.h"
#include "tunnel_protocol.h"
#include "tunnel_message_reconstructor.h"
#include "tcp_connection_manager.h"
#include "sockaddr.h"
#include "udp.h"
#include "verbose.h"

static const size_t max_buffer_size = 100000;
static const size_t max_tcp_data_size = 16384;  // 16KB per TCP read

char udp_packet_buffer[max_buffer_size];
char tcp_data_buffer[max_tcp_data_size];
char tcp_tunnel_buffer[max_buffer_size];

TunnelMessageReconstructor reconstructor;

// Static TCP connection manager - preserved across reconnections
static TCPConnectionManager tcp_connections;

// Helper function to send a tunnel message back to server
// Returns true on success, false on failure
bool sendTunnelMessage(const std::unique_ptr<TCPSocket>& tunnel, 
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
    int sent = tunnel->send(&header, TunnelProtocol::HEADER_SIZE);
    if (sent != TunnelProtocol::HEADER_SIZE)
    {
        LOG_ERROR << "Failed to send message header" << std::endl;
        return false;
    }
    
    // Send payload (if any)
    if (payload_len > 0)
    {
        sent = tunnel->send(payload, payload_len);
        if (sent != payload_len)
        {
            LOG_ERROR << "Failed to send message payload" << std::endl;
            return false;
        }
    }
    
    return true;
}

// Dispatch loop for TunnelClient
// Returns true if user requested quit (Q pressed)
// Returns false if connection was lost (should reconnect)
bool dispatch_loop( const std::unique_ptr<TCPSocket>& tunnel,
                    UDPSocket& udp_forwarder,
                    std::shared_ptr<TCPSocket> webSock,
                    const SockAddr& dest_udp,
                    const SockAddr& dest_tcp )
{
    fd_set read_fds;
    int    fd_max = 0;
    bool   cont_loop = true;
    bool   user_quit = false;  // Track if user requested quit

    std::vector<int> read_sockets;
    read_sockets.push_back( 0 ); // stdin
    read_sockets.push_back( tunnel->socket() );
    read_sockets.push_back( udp_forwarder.socket() );
    
    // Log existing TCP connections on entry (after reconnection)
    if (tcp_connections.connectionCount() > 0)
    {
        LOG_INFO << "Dispatch loop started with " << tcp_connections.connectionCount() 
                 << " existing TCP connections preserved" << std::endl;
        for (uint32_t conn_id : tcp_connections.getAllConnIds())
        {
            LOG_DEBUG << "  Preserved conn_id=" << conn_id << std::endl;
        }
    }

    while( cont_loop )
    {
        FD_ZERO( &read_fds );

        std::ostringstream ostr;
        ostr << "    call select with read fds ";
        for( auto it : read_sockets )
        {
            ostr << it << " ";
            FD_SET( it, &read_fds );
            fd_max = std::max( fd_max, it );
        }
        
        // Add all TCP connection sockets (preserved across reconnections)
        auto tcp_sockets = tcp_connections.getAllSockets();
        for (int sock : tcp_sockets)
        {
            ostr << sock << " ";
            FD_SET( sock, &read_fds );
            fd_max = std::max( fd_max, sock );
        }
        
        LOG_DEBUG << ostr.str() << std::endl;

        int retval = ::select( fd_max+1, &read_fds, nullptr, nullptr, nullptr );

        if (retval < 0)
        {
            if (errno == EINTR)
            {
                // Interrupted by signal, retry
                continue;
            }
            LOG_ERROR << "Select failed: " << strerror(errno) << std::endl;
            break;
        }

        if( FD_ISSET( 0, &read_fds ) )
        {
            int c = getchar( );
            if( c == 'q' || c == 'Q' )
            {
                std::cout << "= Q pressed by user. Quitting." << std::endl;
                user_quit = true;
                cont_loop = false;
            }
        }

        if( FD_ISSET( tunnel->socket(), &read_fds ) )
        {
            int retval = tunnel->recv( tcp_tunnel_buffer, max_buffer_size );
            if( retval < 0 )
            {
                LOG_ERROR << "Error in TCP tunnel, socket " << tunnel->socket() << ". "
                          << strerror(errno) << std::endl;
                LOG_INFO << "Tunnel connection lost. TCP connections will be preserved for reconnection." << std::endl;
                cont_loop = false;
            }
            else if( retval == 0 )
            {
                LOG_INFO << "Tunnel socket closed by server." << std::endl;
                LOG_INFO << "TCP connections will be preserved for reconnection." << std::endl;
                cont_loop = false;
            }
            else
            {
                // Feed received bytes to reconstructor
                reconstructor.collect_from_tunnel( tcp_tunnel_buffer, retval );
                
                // Process all complete messages
                while (reconstructor.hasMessages())
                {
                    TunnelMessage& msg = reconstructor.frontMessage();
                    
                    LOG_DEBUG << "Processing message: type=" 
                              << TunnelProtocol::messageTypeToString(msg.type)
                              << " conn_id=" << msg.conn_id
                              << " payload_size=" << msg.payload.size() << std::endl;
                    
                    switch (msg.type)
                    {
                        case TunnelMessageType::UDP_PACKET:
                        {
                            // Forward UDP packet to destination
                            if (msg.payload.size() > 0)
                            {
                                int sent = udp_forwarder.send(msg.payload.data(), 
                                                             msg.payload.size(), 
                                                             dest_udp);
                                if (sent >= 0)
                                {
                                    LOG_DEBUG << "Forwarded UDP packet of size " 
                                              << msg.payload.size() << " to destination" << std::endl;
                                }
                                else if (errno == EWOULDBLOCK || errno == EAGAIN)
                                {
                                    LOG_WARN << "UDP socket would block - packet dropped" << std::endl;
                                }
                                else
                                {
                                    LOG_ERROR << "Error forwarding UDP packet: " 
                                              << strerror(errno) << std::endl;
                                }
                            }
                            else
                            {
                                // Zero-length UDP packet is valid
                                LOG_DEBUG << "Forwarding zero-length UDP packet" << std::endl;
                                int sent = udp_forwarder.send(msg.payload.data(), 0, dest_udp);
                                if (sent < 0)
                                {
                                    LOG_ERROR << "Error forwarding zero-length UDP packet: " 
                                              << strerror(errno) << std::endl;
                                }
                            }
                            break;
                        }
                        
                        case TunnelMessageType::TCP_OPEN:
                        {
                            LOG_INFO << "TCP_OPEN received for conn_id=" << msg.conn_id << std::endl;
                            
                            // Check if connection already exists (after reconnection)
                            if (tcp_connections.hasConnection(msg.conn_id))
                            {
                                LOG_WARN << "TCP_OPEN for existing conn_id=" << msg.conn_id 
                                         << " - connection already preserved from before reconnection" << std::endl;
                                // Connection already exists, don't create new one
                                break;
                            }
                            
                            // Create outgoing TCP connection to destination
                            std::shared_ptr<TCPSocket> tcp_conn(new TCPSocket(dest_tcp.getAddress().c_str(), 
                                                                               dest_tcp.getPort()));
                            
                            if (tcp_conn->valid())
                            {
                                // Set non-blocking to avoid delaying UDP
                                tcp_conn->setNoBlock();
                                
                                LOG_INFO << "Connected to " << dest_tcp.getAddress() 
                                         << ":" << dest_tcp.getPort() 
                                         << " for conn_id=" << msg.conn_id << std::endl;
                                
                                tcp_connections.addConnection(msg.conn_id, tcp_conn);
                            }
                            else
                            {
                                LOG_ERROR << "Failed to connect to " << dest_tcp.getAddress() 
                                          << ":" << dest_tcp.getPort() 
                                          << " for conn_id=" << msg.conn_id << std::endl;
                                
                                // Send TCP_CLOSE back to server
                                sendTunnelMessage(tunnel, msg.conn_id, 
                                                 TunnelMessageType::TCP_CLOSE, nullptr, 0);
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
                                              << " bytes to destination TCP conn_id=" << msg.conn_id << std::endl;
                                }
                            }
                            else
                            {
                                LOG_WARN << "Received TCP_DATA for unknown conn_id=" << msg.conn_id << std::endl;
                                sendTunnelMessage(tunnel, msg.conn_id, 
                                                 TunnelMessageType::TCP_CLOSE, nullptr, 0);
                            }
                            break;
                        }
                        
                        case TunnelMessageType::TCP_CLOSE:
                        {
                            LOG_INFO << "TCP_CLOSE received for conn_id=" << msg.conn_id << std::endl;
                            tcp_connections.removeConnection(msg.conn_id);
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

        if( FD_ISSET( udp_forwarder.socket(), &read_fds ) )
        {
            // Receive UDP response from the destination
            SockAddr response_sender;
            int retval = udp_forwarder.recv( udp_packet_buffer, max_buffer_size, response_sender );
            
            if (retval > 0)
            {
                LOG_DEBUG << "Received UDP response (" << retval 
                          << " bytes) from destination " 
                          << response_sender.getAddress() << ":" << response_sender.getPort() 
                          << std::endl;
                
                // Send response back through tunnel to TunnelServer
                bool success = sendTunnelMessage(tunnel,
                                                0,  // conn_id = 0 for UDP
                                                TunnelMessageType::UDP_PACKET,
                                                udp_packet_buffer,
                                                retval);
                
                if (success)
                {
                    LOG_DEBUG << "Sent UDP response back through tunnel" << std::endl;
                }
                else
                {
                    LOG_ERROR << "Failed to send UDP response through tunnel" << std::endl;
                    LOG_INFO << "Tunnel connection lost while sending. Will reconnect." << std::endl;
                    cont_loop = false;
                }
            }
            else if (retval < 0)
            {
                if (errno != EWOULDBLOCK && errno != EAGAIN)
                {
                    LOG_ERROR << "Error receiving UDP response: " << strerror(errno) << std::endl;
                }
            }
        }

        // Check all TCP connections to destination for data
        // These connections are preserved across tunnel reconnections
        for (uint32_t conn_id : tcp_connections.getAllConnIds())
        {
            auto* conn = tcp_connections.getConnection(conn_id);
            if (!conn || !conn->socket || !conn->valid)
                continue;
                
            if (FD_ISSET(conn->socket->socket(), &read_fds))
            {
                int bytes = conn->socket->recv(tcp_data_buffer, max_tcp_data_size);
                
                if (bytes == 0)
                {
                    // Connection closed by destination
                    LOG_INFO << "Destination TCP connection closed, conn_id=" << conn_id << std::endl;
                    
                    // Try to send TCP_CLOSE through tunnel
                    // If tunnel is down, this will fail but connection will be cleaned up
                    sendTunnelMessage(tunnel, conn_id, TunnelMessageType::TCP_CLOSE, nullptr, 0);
                    tcp_connections.removeConnection(conn_id);
                }
                else if (bytes < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        // Non-blocking socket, no data available
                        continue;
                    }
                    
                    LOG_WARN << "Error reading from destination TCP conn_id=" << conn_id 
                             << ": " << strerror(errno) << std::endl;
                    
                    sendTunnelMessage(tunnel, conn_id, TunnelMessageType::TCP_CLOSE, nullptr, 0);
                    tcp_connections.removeConnection(conn_id);
                }
                else
                {
                    // Data received from destination
                    LOG_DEBUG << "Received " << bytes << " bytes from destination TCP conn_id=" << conn_id << std::endl;
                    
                    bool success = sendTunnelMessage(tunnel, 
                                                     conn_id,
                                                     TunnelMessageType::TCP_DATA,
                                                     tcp_data_buffer,
                                                     bytes);
                    
                    if (!success)
                    {
                        LOG_ERROR << "Failed to send TCP_DATA for conn_id=" << conn_id << std::endl;
                        LOG_INFO << "Tunnel connection lost while sending TCP data. Will reconnect." << std::endl;
                        // Don't remove connection - it will be preserved for reconnection
                        cont_loop = false;
                        break;  // Exit loop to trigger reconnection
                    }
                }
            }
        }
    }
    
    // Cleanup before exiting dispatch loop
    if (user_quit)
    {
        LOG_INFO << "User quit - closing all TCP connections" << std::endl;
        
        // Close all TCP connections gracefully on user quit
        for (uint32_t conn_id : tcp_connections.getAllConnIds())
        {
            LOG_DEBUG << "Closing TCP connection conn_id=" << conn_id << std::endl;
            tcp_connections.removeConnection(conn_id);
        }
    }
    else
    {
        LOG_INFO << "Tunnel disconnected - preserving " 
                 << tcp_connections.connectionCount() 
                 << " TCP connections for reconnection" << std::endl;
        // TCP connections are NOT removed - they remain in tcp_connections
        // They will continue to be monitored after reconnection
    }
    
    // Clear any remaining data in reconstructor
    LOG_DEBUG << "Flushing reconstructor buffer. Remaining messages: " 
              << reconstructor.messageCount() << std::endl;
    
    // Return true if user quit, false if connection lost
    return user_quit;
}


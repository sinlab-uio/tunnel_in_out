#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include <sys/select.h>
#include <unistd.h> // for close
#include <string.h> // for strerror
#include <errno.h>

#include "tunnel_client_dispatch.h"
#include "tunnel_protocol.h"
#include "tunnel_message_reconstructor.h"
#include "sockaddr.h"
#include "udp.h"
#include "verbose.h"

static const size_t max_buffer_size = 100000;

char udp_packet_buffer[max_buffer_size];
char tcp_websock_buffer[max_buffer_size];
char tcp_tunnel_buffer[max_buffer_size];

static const bool verbose = false;

TunnelMessageReconstructor reconstructor;

// Helper function to send a tunnel message back to server
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
        LOG_ERROR << "ERROR: Payload too large: " << payload_len << std::endl;
        return false;
    }
    
    // Create header
    TunnelMessageHeader header;
    TunnelProtocol::createHeader(header, conn_id, payload_len, type);
    
    // Send header
    int sent = tunnel.send(&header, TunnelProtocol::HEADER_SIZE);
    if (sent != TunnelProtocol::HEADER_SIZE)
    {
        LOG_ERROR << __FILE__ << ":" << __LINE__ 
                  << " Failed to send message header" << std::endl;
        return false;
    }
    
    // Send payload (if any)
    if (payload_len > 0)
    {
        sent = tunnel.send(payload, payload_len);
        if (sent != payload_len)
        {
            LOG_ERROR << __FILE__ << ":" << __LINE__ 
                      << " Failed to send message payload" << std::endl;
            return false;
        }
    }
    
    return true;
}

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
        // Note: Write blocking is now handled per-message, not globally

        FD_ZERO( &read_fds );
        FD_ZERO( &write_fds );

        std::ostringstream ostr;
        ostr << "    call select with read fds ";
        for( auto it : read_sockets )
        {
            ostr << it << " ";
            FD_SET( it, &read_fds );
            fd_max = std::max( fd_max, it );
        }
        ostr << ", write fds ";
        for( auto it : write_sockets )
        {
            ostr << it << " ";
            FD_SET( it, &write_fds );
            fd_max = std::max( fd_max, it );
        }
        LOG_DEBUG << ostr.str() <<  std::endl;

        int retval = ::select( fd_max+1, &read_fds, &write_fds, nullptr, nullptr );

        if( FD_ISSET( 0, &read_fds ) )
        {
            LOG_DEBUG << std::endl;
            int c = getchar( );
            if( c == 'q' || c == 'Q' )
            {
                std::cout << "= Q pressed by user. Quitting." << std::endl;
                cont_loop = false;
            }
        }

        if( FD_ISSET( tunnel.socket(), &read_fds ) )
        {
            LOG_DEBUG << std::endl;
            int retval = tunnel.recv( tcp_tunnel_buffer, max_buffer_size );
            if( retval < 0 )
            {
                LOG_ERROR << "Error in TCP tunnel, socket " << tunnel.socket() << ". "
                          << strerror(errno) << std::endl;
                cont_loop = false;
            }
            else if( retval == 0 )
            {
                std::cout << "= Socket " << tunnel.socket() << " closed. Quitting." << std::endl;
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
                            // Forward UDP packet to destination (request from outside)
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
                                    LOG_DEBUG << "UDP socket would block - packet dropped" << std::endl;
                                    // In future: could queue for retry
                                }
                                else
                                {
                                    LOG_ERROR << "Error forwarding UDP packet: " << strerror(errno) << std::endl;
                                }
                            }
                            else
                            {
                                // Zero-length UDP packet is valid
                                LOG_DEBUG << "Forwarding zero-length UDP packet" << std::endl;
                                int sent = udp_forwarder.send(msg.payload.data(), 
                                                             0, 
                                                             dest_udp);
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
                            LOG_ERROR << "TODO: Handle TCP_OPEN for conn_id " 
                                      << msg.conn_id << std::endl;
                            // Future: Create outgoing TCP connection to dest_tcp
                            break;
                        }
                        
                        case TunnelMessageType::TCP_DATA:
                        {
                            LOG_ERROR << "TODO: Handle TCP_DATA for conn_id " 
                                      << msg.conn_id << std::endl;
                            // Future: Forward data to corresponding TCP connection
                            break;
                        }
                        
                        case TunnelMessageType::TCP_CLOSE:
                        {
                            LOG_ERROR << "TODO: Handle TCP_CLOSE for conn_id " 
                                      << msg.conn_id << std::endl;
                            // Future: Close corresponding TCP connection
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
            LOG_DEBUG << std::endl;
            
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
                    LOG_WARN << "Failed to send UDP response through tunnel" << std::endl;
                    // Connection might be broken
                    cont_loop = false;
                }
            }
            else if (retval < 0)
            {
                if (errno != EWOULDBLOCK && errno != EAGAIN)
                {
                    LOG_WARN << "Error receiving UDP response: " << strerror(errno) << std::endl;
                }
            }
        }

        if( webSock && FD_ISSET( webSock->socket(), &read_fds ) )
        {
            LOG_DEBUG << std::endl;
            webSock->recv( tcp_websock_buffer, max_buffer_size );
        }

        if( FD_ISSET( udp_forwarder.socket(), &write_fds ) )
        {
            LOG_DEBUG << std::endl;
            // Write unblocking handled per-message now
        }
    }
}


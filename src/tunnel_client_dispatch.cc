#include <iostream>
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

static const size_t max_buffer_size = 100000;

char udp_packet_buffer[max_buffer_size];
char tcp_websock_buffer[max_buffer_size];
char tcp_tunnel_buffer[max_buffer_size];

static const bool verbose = false;

TunnelMessageReconstructor reconstructor;

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
    
    // Set reconstructor verbosity
    reconstructor.setVerbose(verbose);

    while( cont_loop )
    {
        std::vector<int> write_sockets;
        // Note: Write blocking is now handled per-message, not globally

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
                // Feed received bytes to reconstructor
                reconstructor.collect_from_tunnel( tcp_tunnel_buffer, retval );
                
                // Process all complete messages
                while (reconstructor.hasMessages())
                {
                    TunnelMessage& msg = reconstructor.frontMessage();
                    
                    std::cerr << "Processing message: type=" 
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
                                    std::cerr << "Forwarded UDP packet of size " 
                                              << msg.payload.size() << std::endl;
                                }
                                else if (errno == EWOULDBLOCK || errno == EAGAIN)
                                {
                                    std::cerr << "UDP socket would block - packet dropped" << std::endl;
                                    // In future: could queue for retry
                                }
                                else
                                {
                                    std::cerr << "Error forwarding UDP packet: " 
                                              << strerror(errno) << std::endl;
                                }
                            }
                            break;
                        }
                        
                        case TunnelMessageType::TCP_OPEN:
                        {
                            std::cerr << "TODO: Handle TCP_OPEN for conn_id " 
                                      << msg.conn_id << std::endl;
                            // Future: Create outgoing TCP connection to dest_tcp
                            break;
                        }
                        
                        case TunnelMessageType::TCP_DATA:
                        {
                            std::cerr << "TODO: Handle TCP_DATA for conn_id " 
                                      << msg.conn_id << std::endl;
                            // Future: Forward data to corresponding TCP connection
                            break;
                        }
                        
                        case TunnelMessageType::TCP_CLOSE:
                        {
                            std::cerr << "TODO: Handle TCP_CLOSE for conn_id " 
                                      << msg.conn_id << std::endl;
                            // Future: Close corresponding TCP connection
                            break;
                        }
                        
                        default:
                            std::cerr << "Unknown message type: " 
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
            std::cerr << __LINE__ << std::endl;
            // This receives responses from the destination
            // Currently not forwarded back through tunnel
            int retval = udp_forwarder.recv( udp_packet_buffer, max_buffer_size );
            if (retval > 0)
            {
                std::cerr << "Received " << retval 
                          << " bytes response on UDP forwarder (not forwarded back)" << std::endl;
            }
        }

        if( webSock && FD_ISSET( webSock->socket(), &read_fds ) )
        {
            std::cerr << __LINE__ << std::endl;
            webSock->recv( tcp_websock_buffer, max_buffer_size );
        }

        if( FD_ISSET( udp_forwarder.socket(), &write_fds ) )
        {
            std::cerr << __LINE__ << std::endl;
            // Write unblocking handled per-message now
        }
    }
}


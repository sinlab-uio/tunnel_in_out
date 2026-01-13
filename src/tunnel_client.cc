#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include <argp.h>

#include <unistd.h> // for close
#include <time.h>   // for time()

#include "tunnel_client_argp.h"
#include "tunnel_client_dispatch.h"
#include "sockaddr.h"
#include "udp.h"
#include "tcp.h"
#include "verbose.h"

// Attempt to connect to TunnelServer with retry
// Returns valid TCPSocket or invalid socket if max retries exceeded
TCPSocket* connectWithRetry(const std::string& host, uint16_t port, 
                           int max_attempts = 5, int delay_seconds = 2)
{
    for (int attempt = 1; attempt <= max_attempts; attempt++)
    {
        LOG_INFO << "Connection attempt " << attempt << " of " << max_attempts 
                 << " to " << host << ":" << port << std::endl;
        
        TCPSocket* tunnel = new TCPSocket(host, port);
        
        if (tunnel->valid())
        {
            LOG_INFO << "Successfully connected to " << host << ":" << port 
                     << " on socket " << tunnel->socket() << std::endl;
            return tunnel;
        }
        delete tunnel;
        
        LOG_WARN << "Connection attempt " << attempt << " failed" << std::endl;
        
        if (attempt < max_attempts)
        {
            LOG_INFO << "Waiting " << delay_seconds << " seconds before retry..." << std::endl;
            sleep(delay_seconds);
        }
    }
    
    LOG_ERROR << "Failed to connect after " << max_attempts << " attempts" << std::endl;
    return nullptr;
}

int main( int argc, char* argv[] )
{
    arguments args;

    callArgParse( argc, argv, args );

    std::cout << "= ======================" << std::endl;
    std::cout << "= ==== TunnelClient ====" << std::endl;
    std::cout << "= ======================" << std::endl;
    std::cout << "= Press Q<ret> to quit" << std::endl;

    UDPSocket udp_forwarder;
    if( udp_forwarder.create() == false )
    {
        LOG_ERROR << "Failed to create UDP socket for the forwarder (quitting)" << std::endl;
        return -1;
    }
    std::cout << "= Anonymous forwarding socket " << udp_forwarder.socket() << " created" << std::endl;
    udp_forwarder.setNoBlock(); // collection should proceed if the UDP socket is temporarily blocked

    std::shared_ptr<TCPSocket> webSock;

    SockAddr dest_udp( args.forward_udp_host.c_str(), args.forward_udp_port );
    SockAddr dest_tcp( args.forward_tcp_host.c_str(), args.forward_tcp_port );

    // Main reconnection loop
    bool continue_running = true;
    int reconnect_count = 0;
    
    while (continue_running)
    {
        // Connect to TunnelServer with retry
        TCPSocket* tunnel = connectWithRetry(args.tunnel_host, args.tunnel_port);
        LOG_INFO << "connectWithRetry returned" << std::endl;
        
        if (!tunnel || !tunnel->valid())
        {
            LOG_ERROR << "Could not establish tunnel connection. Exiting." << std::endl;
	    if(tunnel) delete tunnel;
            return -1;
        }
        LOG_INFO << "valid tunnel established on socket " << tunnel->socket() << std::endl;
        
        reconnect_count++;
        if (reconnect_count > 1)
        {
            std::cout << "= Reconnection #" << (reconnect_count - 1) 
                      << " established" << std::endl;
        }
        else
        {
            std::cout << "= Initial connection established" << std::endl;
        }
        
        std::cout << "= Connected to " << args.tunnel_host << ":" << args.tunnel_port
                  << " on socket " << tunnel->socket() << std::endl;
        
        // Run dispatch loop
        // Returns true if user requested quit, false if connection lost
        bool user_quit = dispatch_loop( *tunnel, udp_forwarder, webSock, 
                                       dest_udp, dest_tcp );
        

	delete tunnel;

        if (user_quit)
        {
            std::cout << "= User requested quit. Exiting." << std::endl;
            continue_running = false;
        }
        else
        {
            std::cout << "= Tunnel connection lost. Attempting reconnection..." << std::endl;
            LOG_INFO << "Tunnel disconnected. Will attempt to reconnect." << std::endl;
            
            // Brief pause before reconnecting
            sleep(1);
        }
    }
    
    std::cout << "= TunnelClient shutting down" << std::endl;
    if (reconnect_count > 1)
    {
        std::cout << "= Total reconnections: " << (reconnect_count - 1) << std::endl;
    }
    
    return 0;
}


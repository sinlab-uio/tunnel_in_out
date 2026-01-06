#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include <argp.h>

#include <unistd.h> // for close

#include "tunnel_in_argp.h"
#include "tunnel_in_dispatch.h"
#include "sockaddr.h"
#include "udp.h"
#include "tcp.h"

int main( int argc, char* argv[] )
{
    arguments args;

    callArgParse( argc, argv, args );

    std::cout << "= ===================" << std::endl;
    std::cout << "= ==== TunnelIn =====" << std::endl;  // FIXED: was "TunnelOut"
    std::cout << "= ===================" << std::endl;
    std::cout << "= Start this program first" << std::endl;
    std::cout << "= Press Q<ret> to quit" << std::endl;

    TCPSocket tunnel_listener( args.tunnel_tcp );
    if( tunnel_listener.valid() == false )
    {
        std::cerr << "Failed to bind the tunnel listening socket to port " << args.tunnel_tcp << " (quitting)" << std::endl;
        return -1;
    }
    std::cout << "= Waiting for TCP connection from TunnelOut on port " << tunnel_listener.getPort() << ", socket " << tunnel_listener.socket() << std::endl;

    UDPSocket outside_udp( args.outside_udp );
    if( outside_udp.valid() == false )
    {
        std::cerr << "Failed to bind the outside UDP socket to port " << args.outside_udp << " (quitting)" << std::endl;
        return -1;
    }
    std::cout << "= Waiting for UDP packets from the outside on port " << outside_udp.getPort()
              << ", socket " << outside_udp.socket() << std::endl;

    TCPSocket outside_tcp_listener( args.outside_tcp );
    if( outside_tcp_listener.valid() == false )
    {
        std::cerr << "Failed to bind the outside TCP listening socket to port " << args.outside_tcp << " (quitting)" << std::endl;
        return -1;
    }
    std::cout << "= Listening for TCP connection from the outside on port "
              << outside_tcp_listener.getPort()
              << ", socket " << outside_tcp_listener.socket() << std::endl;

    // SockAddr remoteAddress( "localhost", args.outside_udp );
    // remoteAddress.print( std::cout ) << std::endl;
    //
    // SockAddr googleDNS( "8.8.8.8", args.outside_tcp );
    // googleDNS.print( std::cout ) << std::endl;
    //
    // SockAddr heise( "www.heise.de", 80 );
    // heise.print( std::cout ) << std::endl;

    std::shared_ptr<TCPSocket> tunnel;
    std::shared_ptr<TCPSocket> webSock;

    dispatch_loop( tunnel_listener, outside_udp, outside_tcp_listener, tunnel, webSock );
    
    std::cout << "= TunnelIn shutting down" << std::endl;
    return 0;
}


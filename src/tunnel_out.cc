#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include <argp.h>

#include <unistd.h> // for close

#include "tunnel_out_argp.h"
#include "tunnel_out_dispatch.h"
#include "sockaddr.h"
#include "udp.h"
#include "tcp.h"

int main( int argc, char* argv[] )
{
    arguments args;

    callArgParse( argc, argv, args );

    std::cerr << "= ===================" << std::endl;
    std::cerr << "= ==== TunnelOut ====" << std::endl;
    std::cerr << "= ===================" << std::endl;
    std::cout << "= Press Q<ret> to quit" << std::endl;

    TCPSocket tunnel( args.tunnel_host, args.tunnel_port );
    if( tunnel.valid() == false )
    {
        std::cerr << "Failed to connect the tunnel to "
                  << args.tunnel_host << ":" << args.tunnel_port
                  << " (quitting)" << std::endl;
        return -1;
    }

    std::cerr << "= Connected to " << args.tunnel_host << ":" << args.tunnel_port
              << " on socket " << tunnel.socket() << std::endl;

    UDPSocket udp_forwarder;
    if( udp_forwarder.create() == false )
    {
        std::cerr << "Failed to create UDP socket for the forwarder (quitting)" << std::endl;
        return -1;
    }
    std::cout << "= Anonymous forwarding socket " << udp_forwarder.socket() << " created" << std::endl;
    udp_forwarder.setNoBlock(); // collection should proceed if the UDP socket is temporarily blocked

    std::shared_ptr<TCPSocket> webSock;

    SockAddr dest_udp( args.forward_host.c_str(), args.forward_udp_port );
    SockAddr dest_tcp( args.forward_host.c_str(), args.forward_tcp_port );

    dispatch_loop( tunnel, udp_forwarder, webSock, dest_udp, dest_tcp );
}


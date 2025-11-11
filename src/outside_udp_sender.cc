#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include <argp.h>

#include <unistd.h> // for close
#include <string.h> // for close

#include "outside_udp_argp.h"
// #include "tunnel_out_dispatch.h"
#include "sockaddr.h"
#include "udp.h"
#include "tcp.h"

char buffer[10001];

int main( int argc, char* argv[] )
{
    arguments args;

    callArgParse( argc, argv, args );

    std::cout << "Trying to send from packets to " << args.tunnel_host << ":" << args.tunnel_port
              << std::endl;

    UDPSocket glove;
    if( glove.create() == false )
    {
        std::cerr << "Failed to create UDP socket for the outside UDP source (quitting)" << std::endl;
        return -1;
    }
    std::cout << "Successfully created an anonymous UDP socket as an outside." << std::endl;

    SockAddr dest( args.tunnel_host.c_str(), args.tunnel_port );

    buffer[0] = 0;
    strcpy( buffer, "Hello world!" );

    glove.send( buffer, strlen(buffer), dest );

    buffer[0] = 0;
    strcat( buffer, "abcdefghijklmnopqrstuvwxy" );
    strcat( buffer, "abcdefghijklmnopqrstuvwxy" );
    strcat( buffer, "abcdefghijklmnopqrstuvwxy" );
    strcat( buffer, "abcdefghijklmnopqrstuvwxy" );

    glove.send( buffer, strlen(buffer), dest );

    for( int i=0; i<9*4; i++ ) strcat( buffer, "abcdefghijklmnopqrstuvwxy" );

    glove.send( buffer, strlen(buffer), dest );

    /* 2000 bytes in the buffer. This is too big for a UDP packet unless the
     * network supports jumbograms.
     */
    for( int i=0; i<10*4; i++ ) strcat( buffer, "abcdefghijklmnopqrstuvwxy" );
    glove.send( buffer, strlen(buffer), dest );

    /* 9000 bytes in the buffer. This is too big for a UDP packet unless the
     * network supports jumbograms.
     */
    for( int i=0; i<7*10*4; i++ ) strcat( buffer, "abcdefghijklmnopqrstuvwxy" );
    glove.send( buffer, strlen(buffer), dest );

    if( args.send_too_long )
    {
        /* 10000 bytes in the buffer.
         * This is guaranteed too big for a UDP packet.
         */
        for( int i=0; i<10*4; i++ ) strcat( buffer, "abcdefghijklmnopqrstuvwxy" );
        glove.send( buffer, strlen(buffer), dest );
    }
}


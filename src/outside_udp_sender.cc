#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include <argp.h>

#include <unistd.h> // for close
#include <string.h> // for close

#include "outside_udp_argp.h"
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
    while( strlen(buffer)<100 ) strncat( buffer, "100,", 10000 ); 
    buffer[99] = '\n';
    buffer[100] = 0;
    glove.send( buffer, strlen(buffer), dest );

    buffer[0] = 0;
    while( strlen(buffer)<500 ) strncat( buffer, "500,", 10000 ); 
    buffer[499] = '\n';
    buffer[500] = 0;
    glove.send( buffer, strlen(buffer), dest );

    /* 2000 bytes in the buffer. This is too big for a UDP packet unless the
     * network supports jumbograms.
     */
    buffer[0] = 0;
    while( strlen(buffer)<2000 ) strncat( buffer, "2000,", 10000 ); 
    buffer[1999] = '\n';
    buffer[2000] = 0;
    glove.send( buffer, strlen(buffer), dest );

    /* 9000 bytes in the buffer. This is too big for a UDP packet unless the
     * network supports jumbograms.
     */
    buffer[0] = 0;
    while( strlen(buffer)<9000 ) strncat( buffer, "9000,", 10000 ); 
    buffer[8999] = '\n';
    buffer[9000] = 0;
    glove.send( buffer, strlen(buffer), dest );

    if( args.send_too_long )
    {
        /* 10000 bytes in the buffer.
         * This is guaranteed too big for a UDP packet.
         */
        buffer[0] = 0;
        while( strlen(buffer)<10000 ) strncat( buffer, "10000,", 10000 ); 
        buffer[9999] = '\n';
        buffer[10000] = 0;
        glove.send( buffer, strlen(buffer), dest );
    }

    buffer[0] = 0;
    strcpy( buffer, "Bye world!" );
    glove.send( buffer, strlen(buffer), dest );
}


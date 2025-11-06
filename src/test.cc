#include <iostream>
#include <string>
// #include <vector>
// #include <cstring> // For memset

// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <unistd.h> // for close
// #include <errno.h>  // for errno

// #include <sys/types.h>
// #include <net/if.h>
// #include <ifaddrs.h>
// #include <sys/types.h>
// #include <netdb.h>

#include "sockaddr.h"
#include "udp.h"

int main( )
{
    SockAddr remoteAddress( "localhost", 3169 );
    remoteAddress.print( std::cout ) << std::endl;

    SockAddr googleDNS( "8.8.8.8", 512 );
    googleDNS.print( std::cout ) << std::endl;

    SockAddr heise( "www.heise.de", 80 );
    heise.print( std::cout ) << std::endl;
}


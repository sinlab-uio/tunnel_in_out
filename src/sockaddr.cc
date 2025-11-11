#include "sockaddr.h"

// #include <iostream>
// #include <string>
// #include <vector>
// #include <cstring> // For memset
#include <string.h>

#include <sys/types.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
#include <arpa/inet.h>
// #include <unistd.h> // for close
// #include <errno.h>  // for errno

#include <netdb.h>

SockAddr::SockAddr( )
{
    memset( &addr, 0, sizeof(sockaddr_in) );
}

SockAddr::SockAddr( uint16_t port )
{
    memset( &addr, 0, sizeof(sockaddr_in) );
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons( port );
}

SockAddr::SockAddr( const char* remoteName, uint16_t port )
{
    addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET; // Request IPv4 addresses
    hints.ai_socktype = 0;       // any type of address, not only SOCK_STREAM for TCP
    hints.ai_protocol = 0;       // any protocol
    hints.ai_flags    = 0;       // N  special flags

    int status = getaddrinfo( remoteName, nullptr, &hints, &result );
    if (status != 0)
    {
        std::cerr << "Could not get IPv4 address for " << remoteName << ": " << gai_strerror(status) << std::endl;
        return;
    }

    // Iterate through the results, although for AF_INET and SOCK_STREAM,
    // the first result is often sufficient.
    for( addrinfo* p = result; p != nullptr; p = p->ai_next )
    {
        if (p->ai_family == AF_INET)
        {
            memcpy( &addr, p->ai_addr, p->ai_addrlen );
            break; // Found an IPv4 address, use it
        }
    }

    // Port has remained uninitialized
    addr.sin_port = htons( port );

    freeaddrinfo( result );
}

sockaddr* SockAddr::get( )
{
    return (sockaddr*)&addr;
}

const sockaddr* SockAddr::get( ) const
{
    return (const sockaddr*)&addr;
}

socklen_t SockAddr::size() const
{
    return sizeof(sockaddr_in);
}

std::string SockAddr::getAddress( ) const
{
    std::string addrString = inet_ntoa( addr.sin_addr );
    return addrString;
}

uint16_t SockAddr::getPort( ) const
{
    return ntohs( addr.sin_port );
}

std::ostream& SockAddr::print( std::ostream& ostr ) const
{
    ostr << getAddress() << ":" << getPort();
    return ostr;
}


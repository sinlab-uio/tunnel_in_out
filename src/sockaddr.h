#pragma once

#include <iostream>
#include <string>
#include <sys/types.h>
#include <netinet/in.h>

class SockAddr
{
public:
    sockaddr_in addr;

    // initialize the sockaddr_in structure to zero
    SockAddr( );

    // initialize the sockaddr_in structure to and ANY address with the given port
    SockAddr( uint16_t port );

    /** initialize the sockaddr_in structure with the given hostname.
     *  The port in the structure is set, but not the protocol.
     *  It is not known whether addr.sin_addr.s_addr is initialized by this.  */
    SockAddr( const char* remoteName, uint16_t port );

    /** Typecast the address to (struct sockaddr*)
     *  Not const to allow use in recvfrom.  */
    sockaddr* get( );

    // Typecast the address to (struct sockaddr*)
    const sockaddr* get( ) const;

    /* The size of the address. For sending and to initialize the parameter
     *  for recvfrom. */
    socklen_t size() const;

    // Return the IPv4 address in dotted decimal notation in the string
    std::string getAddress( ) const;

    // Return the host in host byte order.
    uint16_t getPort( ) const;

    // Print dotted decimal address and port to the given ostream.
    std::ostream& print( std::ostream& ostr ) const;
};


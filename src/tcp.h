#pragma once

// #include <iostream>
// #include <string>
// #include <vector>
// #include <cstring> // For memset

#include <sys/types.h>
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

class TCPSocket
{
    bool     _valid {false};
    uint16_t _port  {0};
    int      _sock  {-1};

public:
    // Create an unconnected client socket
    TCPSocket( ) = default;

    /* Create a server socket that is bound to port.
     * Check valid() to verify if this worked.
     */
    TCPSocket( uint16_t port );

    /* Create a connected socket from a listener socket
     * (accept a connection).
     * Check valid() to verify if this worked.
     */
    TCPSocket( const TCPSocket& listener );

    /* Create a socket and connect it to host:port.
     * Check valid() to verify if this worked.
     */
    TCPSocket( const std::string& host, uint16_t port );

    // Close the socket if it is still valid
    ~TCPSocket( );

    /* Create a TCP socket and bind it to the given port.
     * Store own port in _port.
     */
    bool createServer( uint16_t port );

    /* Create a TCP socket and connect it to the given port.
     * Store own port in _port.
     */
    bool createClient( const char* host, uint16_t port );

    // Close the UDP socket and reset the valid flat
    void destroy( );

    // Integer value of the socket
    int socket() const;

    /* Returns the _port variable. It is not guaranteed that this is
     * the port number that getsockname would return.
     */
    int getPort() const { return _port; }

    // Check if the socket is currently valid
    bool valid() const { return _valid; }

    /* Read from the socket into given buffer, up to buflen bytes.
     * The socket must be valid.
     * The function works differently when the socket is blocking vs when it is
     * non-blocking.
     * Returns the usual socket error codes.
     */
    int recv( char* buffer, size_t buflen );

    /* Write to the socket from the given buffer, up to buflen bytes.
     * The socket must be valid.
     * The function works differently when the socket is blocking vs when it is
     * non-blocking.
     * Returns the usual socket error codes.
     */
    int send( const void* buffer, size_t buflen );
};


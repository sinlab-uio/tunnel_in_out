#pragma once

#include <sys/types.h>

#include "sockaddr.h"

class UDPSocket
{
    bool     _valid {false};
    uint16_t _port  {0};
    int      _sock  {-1};

public:
    // Default object, not initialized
    UDPSocket() = default;

    // Make a server socket. Check valid() to see if it was successful.
    UDPSocket( uint16_t port );

    // Close the socket if it is valid
    ~UDPSocket( );

    // Create a UDP socket and bind it to the given port
    bool createServer( uint16_t port );

    // Create a unbound UDP socke
    bool create( );

    // Close the UDP socket and reset the valid flat
    void destroy( );

    // Check if the socket is currently valid
    bool valid() const { return _valid; }

    // Integer value of the socket
    int socket() const;

    // Set the socket to non-blocking mode.
    void setNoBlock( );

    /* Returns the member variable _port.
     * It is not guaranteed that this is the variable that getsockname
     * would return.
     */
    int getPort() const { return _port; }

    /* Read from the socket into give buffer, up to buflen bytes.
     * The socket must be valid.
     * The function works differently when the socket is blocking vs when it is
     * non-blocking.
     * IP and port of the sending entity are stored in senderAddr. Must be IPv4.
     */
    int recv( char* buffer, size_t buflen, SockAddr& senderAddr );

    // Same as the other recv method, but ignore the sender's address
    int recv( char* buffer, size_t buflen );

    // Send the buffer to the given address and port using this socket
    int send( const char* buffer, size_t buflen, const SockAddr& dest );
};


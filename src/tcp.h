#pragma once

#include <iostream>
#include <string>
#include <stdint.h>

class TCPSocket
{
    int      _sock  { -1 };
    uint16_t _port  { 0 };
    bool     _valid { false };

    /* Create a TCP socket and connect it to the given port.
     * Store own port in _port.
     */
    bool createClient( const char* host, uint16_t port );

    /* Create a TCP socket and bind it to the given port.
     * Store own port in _port.
     */
    bool createServer( uint16_t port );
    
    // Low-latency optimizations
    void setTcpNoDelay();
    void setSocketBuffers(int size);

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

    // Close the UDP socket and reset the valid flat
    void destroy( );

    // Check if the socket is currently valid
    inline bool valid() const { return _valid; }

    // Integer value of the socket
    int socket() const;
   
    /* Returns the _port variable. It is not guaranteed that this is
     * the port number that getsockname would return.
     */
    inline uint16_t getPort() const { return _port; }

    // Set this socket to non-blocking. Important for TCP forwarding.
    void setNoBlock( );

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


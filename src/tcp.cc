// #include <iostream>
// #include <string>
// #include <vector>
// #include <cstring> // For memset

#include <sys/types.h>
#include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
#include <unistd.h> // for close
#include <string.h> // for strerror
// #include <errno.h>  // for errno

#include "sockaddr.h"
#include "tcp.h"

TCPSocket::TCPSocket( uint16_t port )
{
    bool success = createServer( port );
}

TCPSocket::TCPSocket( const TCPSocket& listener )
{
    SockAddr peer;
    socklen_t peerlen;

    peerlen = peer.size();
    _sock = ::accept( listener.socket(), peer.get(), &peerlen );
    if( _sock < 0 )
    {
        std::cerr << "Failed to create TCP socket from a listener" << std::endl;
        return;
    }


    // Reusing the address struct peer. We are actually retrieving our own port.
    peerlen = peer.size();
    int retval = getsockname( _sock, peer.get(), &peerlen );
    if( retval < 0 )
    {
        std::cerr << "Failed to retrieve TCP socket's own port after accept (ignore)" << std::endl;
    }
    else
    {
        _port = peer.getPort();
    }

    std::cerr << "Created TCP server socket " << _sock << " on port " << _port << std::endl;

    _valid = true;
}

TCPSocket::TCPSocket( const std::string& host, uint16_t port )
{
    bool success = createClient( host.c_str(), port );

    if( success )
        _valid = true;
}

TCPSocket::~TCPSocket( )
{
    destroy();
}

bool TCPSocket::createClient( const char* host, uint16_t port )
{
    int retval;

    _sock = ::socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if( _sock < 0 )
    {
        std::cerr << "Failed to create TCP socket" << std::endl;
        return false;
    }

    SockAddr server( host, port );

    retval = connect( _sock, server.get(), server.size() );
    if( retval < 0 )
    {
        std::cerr << "Failed to connect TCP client socket to port " << port
                  << " - " << strerror(errno)
                  << std::endl;
        ::close( _sock );
        return false;
    }

    SockAddr  addr;
    socklen_t addrlen = addr.size();

    retval = getsockname( _sock, addr.get(), &addrlen );
    if( retval < 0 )
    {
        std::cerr << "Failed to retrieve tunnel TCP socket's local port after connect (ignore)" << std::endl;
    }
    else
    {
        _port = addr.getPort();
    }

    _valid = true;
    return true;
}

bool TCPSocket::createServer( uint16_t port )
{
    _sock = ::socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if( _sock < 0 )
    {
        std::cerr << "Failed to create TCP socket" << std::endl;
        return false;
    }

    SockAddr server( port );

    if( ::bind( _sock, server.get(), server.size() ) < 0 )
    {
        std::cerr << "Failed to bind TCP server socket " << _sock << " to port " << port << std::endl;
        ::close( _sock );
        return false;
    }

    if( ::listen( _sock, SOMAXCONN ) < 0 )
    {
        std::cerr << "Failed to set backlog queue for TCP server socket " << _sock << std::endl;
        ::close( _sock );
        return false;
    }

    _port  = port;
    _valid = true;
    return true;
}

void TCPSocket::destroy( )
{
    if( _valid )
    {
        std::cerr << "Closing TCP socket " << _sock << std::endl;
        ::close( _sock );
        _valid = false;
        _sock  = -1;
    }
}

int TCPSocket::socket() const
{
    return _sock;
}

int TCPSocket::recv( char* buffer, size_t buflen )
{
    std::cout << "Waiting for TCP data on port " << _port << std::endl;

    int bytesReceived = ::read( _sock, buffer, buflen-1 );
    if (bytesReceived < 0)
    {
        std::cerr << __PRETTY_FUNCTION__ << "read failed" << std::endl;
        return bytesReceived;
    }

    std::cout << __PRETTY_FUNCTION__ << " read " << bytesReceived << " bytes from socket " << _sock << std::endl;
    return bytesReceived;
}

int TCPSocket::send( const void* buffer, size_t buflen )
{
    std::cout << "Sending " << buflen << " bytes on TCP socket " << _sock << std::endl;

    int bytesSent = ::write( _sock, buffer, buflen );
    if (bytesSent < 0)
    {
        std::cerr << __PRETTY_FUNCTION__ << "write failed" << std::endl;
        return bytesSent;
    }

    std::cout << __PRETTY_FUNCTION__ << " wrote " << bytesSent << " bytes to socket " << _sock << std::endl;
    return bytesSent;
}


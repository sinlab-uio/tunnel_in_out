#include <unistd.h> // for close
#include <string.h> // for strerror
#include <fcntl.h>

#include "verbose.h"
#include "sockaddr.h"
#include "udp.h"

UDPSocket::UDPSocket( uint16_t port )
{
    createServer( port );
}

UDPSocket::~UDPSocket( )
{
    destroy();
}

bool UDPSocket::createServer( uint16_t port )
{
    _sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if( _sock < 0 )
    {
        LOG_WARN << "Failed to create UDP socket" << std::endl;
        return false;
    }

    SockAddr server( port );

    int retval = bind( _sock, server.get(), server.size() );

    struct sockaddr_in* q = (struct sockaddr_in*)(server.get());
    uint16_t            p = ntohs( q->sin_port );

    if( retval < 0 )
    {
        LOG_WARN << "Failed to bind UDP socket to port " << port << std::endl;
        close( _sock );
        return false;
    }

    _port  = port;
    _valid = true;

    return true;
}

bool UDPSocket::create( )
{
    _sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if( _sock < 0 )
    {
        LOG_WARN << "Failed to create UDP socket" << std::endl;
        return false;
    }

    _port  = 0;
    _valid = true;

    return true;
}

void UDPSocket::destroy( )
{
    if( _valid )
    {
        LOG_DEBUG << "Closing UDP socket " << _sock << std::endl;
        close( _sock );
        _valid = false;
        _sock  = -1;
    }
}

int UDPSocket::socket() const
{
    return _sock;
}

void UDPSocket::setNoBlock( )
{
    /* In attempting to set the socket non-blocking, we ignore
     * error because we cannot do anything about them anyway.
     */
    int flags = fcntl( _sock, F_GETFL, 0 );
    if( flags == -1 ) return;

    flags |= O_NONBLOCK;

    fcntl( _sock, F_SETFL, flags );
}

int UDPSocket::recv( char* buffer, size_t buflen, SockAddr& clientAddr )
{
    socklen_t clientAddrLen = clientAddr.size();

    LOG_DEBUG << "Read UDP packet arriving on port " << _port << std::endl;

    int bytesReceived = recvfrom( _sock, buffer, buflen, 0, clientAddr.get(), &clientAddrLen );
    if (bytesReceived < 0)
    {
        LOG_WARN << "recvfrom failed" << std::endl;
        return bytesReceived;
    }

    LOG_DEBUG << "Received " << bytesReceived << " bytes from " << clientAddr.getAddress() << ":" << clientAddr.getPort() << std::endl;
    return bytesReceived;
}

int UDPSocket::recv( char* buffer, size_t buflen )
{
    SockAddr senderAddr;
    return recv( buffer, buflen, senderAddr );
}

int UDPSocket::send( const char* buffer, size_t buflen, const SockAddr& dest )
{
    if( buffer == nullptr )
    {
        LOG_DEBUG << "calling with buffer that is NULL" << std::endl;
        return -1;
    }

    LOG_DEBUG << "Sending " << buflen << " bytes from UDP packets port " << _port << std::endl;

    int bytesSent = sendto( _sock, buffer, buflen, 0, dest.get(), dest.size() );
    if (bytesSent < 0)
    {
        LOG_DEBUG << "sendto " << buflen << " bytes to " << dest << " failed: " << strerror(errno) << std::endl;
        return bytesSent;
    }

    LOG_DEBUG << "Sent " << bytesSent << " bytes to " << dest.getAddress() << ":" << dest.getPort() << std::endl;
    return bytesSent;
}


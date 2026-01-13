#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>  // For TCP_NODELAY

#include <fcntl.h>
#include <unistd.h> // for close
#include <string.h> // for strerror
#include <errno.h>

#include "sockaddr.h"
#include "tcp.h"
#include "verbose.h"

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
        LOG_ERROR << "Failed to create TCP socket from a listener, reason: " << strerror(errno) << std::endl;
        return;
    }

    // CRITICAL FOR LOW LATENCY: Disable Nagle's algorithm on accepted connections
    setTcpNoDelay();

    // Reusing the address struct peer. We are actually retrieving our own port.
    peerlen = peer.size();
    int retval = getsockname( _sock, peer.get(), &peerlen );
    if( retval < 0 )
    {
        LOG_WARN << "Failed to retrieve TCP socket's own port after accept (ignore)" << std::endl;
    }
    else
    {
        _port = peer.getPort();
    }

    LOG_DEBUG << "Created TCP server socket " << _sock << " on port " << _port << std::endl;

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

void TCPSocket::setTcpNoDelay()
{
    // Disable Nagle's algorithm for low latency
    // This sends packets immediately instead of buffering them
    int flag = 1;
    if (setsockopt(_sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)) < 0) {
        LOG_WARN << "Failed to set TCP_NODELAY - latency may be higher" << std::endl;
    }
}

void TCPSocket::setSocketBuffers(int size)
{
    // Increase socket buffers for better burst handling
    if (setsockopt(_sock, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) < 0) {
        LOG_WARN << "Failed to set SO_SNDBUF" << std::endl;
    }
    if (setsockopt(_sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) < 0) {
        LOG_WARN << "Failed to set SO_RCVBUF" << std::endl;
    }
}

bool TCPSocket::createClient( const char* host, uint16_t port )
{
    int retval;

    _sock = ::socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if( _sock < 0 )
    {
        LOG_ERROR << "Failed to create TCP socket" << std::endl;
        return false;
    }

    // CRITICAL FOR LOW LATENCY: Disable Nagle's algorithm
    setTcpNoDelay();
    
    // Increase socket buffers for better burst handling (1MB)
    setSocketBuffers(1024 * 1024);

    SockAddr server( host, port );

    retval = connect( _sock, server.get(), server.size() );
    if( retval < 0 )
    {
        LOG_ERROR << "Failed to connect TCP client socket to port " << port
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
        LOG_WARN << "Failed to retrieve tunnel TCP socket's local port after connect (ignore)" << std::endl;
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
        LOG_ERROR << "Failed to create TCP socket" << std::endl;
        return false;
    }

    // Enable SO_REUSEADDR to allow quick restarts
    int optval = 1;
    if( ::setsockopt( _sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval) ) < 0 )
    {
        LOG_WARN << "Failed to set SO_REUSEADDR on TCP socket" << std::endl;
    }

    SockAddr server( port );

    if( ::bind( _sock, server.get(), server.size() ) < 0 )
    {
        LOG_ERROR << "Failed to bind TCP server socket " << _sock << " to port " << port << std::endl;
        ::close( _sock );
        return false;
    }

    if( ::listen( _sock, SOMAXCONN ) < 0 )
    {
        LOG_ERROR << "Failed to set backlog queue for TCP server socket " << _sock << std::endl;
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
        LOG_DEBUG << "Closing TCP socket " << _sock << std::endl;
        ::close( _sock );
        _valid = false;
        _sock  = -1;
    }
}

int TCPSocket::socket() const
{
    return _sock;
}

void TCPSocket::setNoBlock( )
{
    /* In attempting to set the socket non-blocking, we ignore
     * error because we cannot do anything about them anyway.
     */
    int flags = fcntl( _sock, F_GETFL, 0 );
    if( flags == -1 ) return;

    flags |= O_NONBLOCK;

    fcntl( _sock, F_SETFL, flags );
}

int TCPSocket::recv( char* buffer, size_t buflen )
{
    LOG_DEBUG << "wait for data on TCP socket " << _sock 
              << ", port " << _port 
              << " (up to " << buflen << " bytes)" << std::endl;

    int bytesReceived = ::read( _sock, buffer, buflen );
    if (bytesReceived < 0)
    {
        LOG_WARN << "failed to read from TCP socket " << _sock 
                 << " with error msg " << strerror(errno) << std::endl;
        return bytesReceived;
    }

    LOG_DEBUG << "read " << bytesReceived 
              << " bytes from TCP socket " << _sock << std::endl;
    return bytesReceived;
}

int TCPSocket::send( const void* buffer, size_t buflen )
{
    LOG_DEBUG << "sending " << buflen 
              << " bytes on TCP socket " << _sock << std::endl;

    size_t totalSent = 0;
    const char* buf = static_cast<const char*>(buffer);
    
    while (totalSent < buflen)
    {
        int bytesSent = ::write( _sock, buf + totalSent, buflen - totalSent );
        
        if (bytesSent < 0)
        {
            if (errno == EINTR)
            {
                // Interrupted by signal, retry
                continue;
            }
            else if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                // Socket buffer full, but we're in blocking mode so this shouldn't happen
                LOG_WARN << " unexpected EWOULDBLOCK on blocking socket" << std::endl;
                return -1;
            }
            else
            {
                LOG_ERROR << " failed to send " << buflen 
                          << " bytes on TCP socket " << _sock 
                          << " with error msg " << strerror(errno) << std::endl;
                return -1;
            }
        }
        else if (bytesSent == 0)
        {
            // Connection closed
            LOG_WARN << "Connection closed during send" << std::endl;
            return -1;
        }
        
        totalSent += bytesSent;
    }

    LOG_DEBUG << "sent " << totalSent 
              << " bytes on TCP socket " << _sock << std::endl;
    return totalSent;
}


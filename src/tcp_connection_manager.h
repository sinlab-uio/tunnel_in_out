#pragma once

#include <map>
#include <memory>
#include <vector>
#include "tcp.h"
#include "sockaddr.h"

// Manages multiple TCP connections through the tunnel
// Each connection has a unique conn_id
class TCPConnectionManager
{
public:
    struct Connection
    {
        uint32_t conn_id;
        std::shared_ptr<TCPSocket> socket;
        bool valid;
        
        Connection(uint32_t id, std::shared_ptr<TCPSocket> sock)
            : conn_id(id), socket(sock), valid(true) {}
    };
    
private:
    std::map<uint32_t, Connection> _connections;  // conn_id -> Connection
    std::map<int, uint32_t> _socket_to_conn_id;   // socket fd -> conn_id
    uint32_t _next_conn_id;
    
public:
    TCPConnectionManager();
    
    // Allocate a new connection ID
    uint32_t allocateConnId();
    
    // Add a new connection
    void addConnection(uint32_t conn_id, std::shared_ptr<TCPSocket> socket);
    
    // Remove a connection
    void removeConnection(uint32_t conn_id);
    
    // Get connection by conn_id
    Connection* getConnection(uint32_t conn_id);
    
    // Get conn_id by socket fd
    uint32_t getConnId(int socket_fd);
    
    // Get all connection IDs
    std::vector<uint32_t> getAllConnIds() const;
    
    // Get all socket fds
    std::vector<int> getAllSockets() const;
    
    // Check if connection exists
    bool hasConnection(uint32_t conn_id) const;
    
    // Get number of connections
    size_t connectionCount() const;
    
    // Mark connection as invalid (but don't remove yet)
    void markInvalid(uint32_t conn_id);
};

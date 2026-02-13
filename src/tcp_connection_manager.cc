#include "tcp_connection_manager.h"

TCPConnectionManager::TCPConnectionManager()
    : _next_conn_id(1)
{}

uint32_t TCPConnectionManager::allocateConnId()
{
    return _next_conn_id++;
}

void TCPConnectionManager::addConnection(uint32_t conn_id, std::unique_ptr<TCPSocket>& socket)
{
    if (socket && socket->valid())
    {
        _socket_to_conn_id[socket->socket()] = conn_id;
        _connections.emplace( conn_id, Connection( conn_id, std::move(socket) ) );
    }
}
    
void TCPConnectionManager::removeConnection(uint32_t conn_id)
{
    auto it = _connections.find(conn_id);
    if (it != _connections.end())
    {
        // Remove from socket mapping
        if (it->second.socket)
        {
            _socket_to_conn_id.erase(it->second.socket->socket());
        }
        _connections.erase(it);
    }
}
    
TCPConnectionManager::Connection* TCPConnectionManager::getConnection(uint32_t conn_id)
{
    auto it = _connections.find(conn_id);
    return (it != _connections.end()) ? &it->second : nullptr;
}
    
uint32_t TCPConnectionManager::getConnId(int socket_fd)
{
    auto it = _socket_to_conn_id.find(socket_fd);
    return (it != _socket_to_conn_id.end()) ? it->second : 0;
}

std::vector<uint32_t> TCPConnectionManager::getAllConnIds() const
{
    std::vector<uint32_t> ids;
    for (const auto& pair : _connections)
    {
        ids.push_back(pair.first);
    }
    return ids;
}
    
std::vector<int> TCPConnectionManager::getAllSockets() const
{
    std::vector<int> sockets;
    for (const auto& pair : _connections)
    {
        if (pair.second.socket && pair.second.valid)
        {
            sockets.push_back(pair.second.socket->socket());
        }
    }
    return sockets;
}
    
bool TCPConnectionManager::hasConnection(uint32_t conn_id) const
{
    return _connections.find(conn_id) != _connections.end();
}
    
size_t TCPConnectionManager::connectionCount() const
{
    return _connections.size();
}
    
void TCPConnectionManager::markInvalid(uint32_t conn_id)
{
    auto it = _connections.find(conn_id);
    if (it != _connections.end())
    {
        it->second.valid = false;
    }
}


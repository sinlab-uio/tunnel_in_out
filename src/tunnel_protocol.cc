#include "tunnel_protocol.h"
#include <arpa/inet.h>
#include <string.h>

// Helper functions for creating and parsing tunnel messages

void TunnelProtocol::createHeader(TunnelMessageHeader& header, 
                                   uint32_t conn_id,
                                   uint16_t length, 
                                   TunnelMessageType type)
{
    header.conn_id = htonl(conn_id);
    header.length = htons(length);
    header.type = htons(static_cast<uint16_t>(type));
}

void TunnelProtocol::parseHeader(const TunnelMessageHeader& header,
                                  uint32_t& conn_id,
                                  uint16_t& length,
                                  TunnelMessageType& type)
{
    conn_id = ntohl(header.conn_id);
    length = ntohs(header.length);
    type = static_cast<TunnelMessageType>(ntohs(header.type));
}

bool TunnelProtocol::isValidMessageType(uint16_t type)
{
    return (type >= static_cast<uint16_t>(TunnelMessageType::UDP_PACKET) &&
            type <= static_cast<uint16_t>(TunnelMessageType::TCP_CLOSE));
}

const char* TunnelProtocol::messageTypeToString(TunnelMessageType type)
{
    switch (type)
    {
        case TunnelMessageType::UDP_PACKET: return "UDP_PACKET";
        case TunnelMessageType::TCP_OPEN:   return "TCP_OPEN";
        case TunnelMessageType::TCP_DATA:   return "TCP_DATA";
        case TunnelMessageType::TCP_CLOSE:  return "TCP_CLOSE";
        default:                            return "UNKNOWN";
    }
}

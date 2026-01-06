#pragma once

#include <stdint.h>
#include <stddef.h>

// Tunnel protocol message types
enum class TunnelMessageType : uint16_t
{
    UDP_PACKET = 1,      // UDP packet data (conn_id = 0 for now)
    TCP_OPEN = 2,        // New TCP connection established
    TCP_DATA = 3,        // TCP stream data
    TCP_CLOSE = 4        // TCP connection closed
};

/* Tunnel message header (8 bytes total)
 * The values in this structure are always in network endian format.
 * 
 * Wire format:
 * +------------------+------------------+------------------+
 * | conn_id (4 bytes)| length (2 bytes) | type (2 bytes)  |
 * +------------------+------------------+------------------+
 * 
 * For UDP_PACKET messages: conn_id is currently unused (set to 0)
 * For TCP messages: conn_id identifies which TCP connection
 */
struct TunnelMessageHeader
{
    uint32_t  conn_id;    // Connection ID (for TCP multiplexing), 0 for UDP
    uint16_t  length;     // Payload length in bytes (max 65535)
    uint16_t  type;       // Message type (TunnelMessageType)
};

// Helper functions for working with the tunnel protocol
namespace TunnelProtocol
{
    // Create a message header (converts to network byte order)
    void createHeader(TunnelMessageHeader& header, 
                     uint32_t conn_id,
                     uint16_t length, 
                     TunnelMessageType type);
    
    // Parse a message header (converts from network byte order)
    void parseHeader(const TunnelMessageHeader& header,
                    uint32_t& conn_id,
                    uint16_t& length,
                    TunnelMessageType& type);
    
    // Check if a message type value is valid
    bool isValidMessageType(uint16_t type);
    
    // Convert message type to string (for debugging)
    const char* messageTypeToString(TunnelMessageType type);
    
    // Constants
    static constexpr size_t HEADER_SIZE = sizeof(TunnelMessageHeader);
    static constexpr uint16_t MAX_PAYLOAD_SIZE = 65535;  // Max UDP packet size
};

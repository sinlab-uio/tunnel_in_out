#include "tunnel_send_message.h"

bool sendTunnelMessage( const std::unique_ptr<TCPSocket>& tunnel, 
                        uint32_t conn_id,
                        TunnelMessageType type,
                        const char* payload,
                        uint16_t payload_len )
{
    // Validate payload length
    if (payload_len > TunnelProtocol::MAX_PAYLOAD_SIZE)
    {
        LOG_ERROR << "Payload too large: " << payload_len << std::endl;
        return false;
    }
    
    // Create header
    TunnelMessageHeader header;
    TunnelProtocol::createHeader(header, conn_id, payload_len, type);
    
    // Send header
    int sent = tunnel->send(&header, TunnelProtocol::HEADER_SIZE);
    if (sent != TunnelProtocol::HEADER_SIZE)
    {
        LOG_ERROR << "Failed to send message header" << std::endl;
        return false;
    }
    
    // Send payload (if any)
    if (payload_len > 0)
    {
        sent = tunnel->send(payload, payload_len);
        if (sent != payload_len)
        {
            LOG_ERROR << "Failed to send message payload" << std::endl;
            return false;
        }
    }
    
    return true;
}


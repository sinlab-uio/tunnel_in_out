#include "tunnel_message_reconstructor.h"
#include "verbose.h"

#include <iostream>
#include <string.h>

void TunnelMessageReconstructor::collect_from_tunnel(const char* buffer, int buflen)
{
    LOG_DEBUG << "Appending " << buflen << " bytes to reconstruction buffer" << std::endl
              << "        Bytes before: " << _bytes_from_tunnel.size() << std::endl;
    
    // Append new bytes to buffer
    _bytes_from_tunnel.insert(_bytes_from_tunnel.end(), buffer, buffer + buflen);
    
    LOG_DEBUG << "Bytes after: " << _bytes_from_tunnel.size() << std::endl;
    
    // Process as many complete messages as possible
    while (!_bytes_from_tunnel.empty())
    {
        if (_wait_for_header)
        {
            // Need complete header (8 bytes)
            if (_bytes_from_tunnel.size() >= TunnelProtocol::HEADER_SIZE)
            {
                LOG_DEBUG << "Reading message header" << std::endl;
                
                // Parse header
                TunnelMessageHeader header;
                memcpy(&header, _bytes_from_tunnel.data(), TunnelProtocol::HEADER_SIZE);
                
                uint32_t conn_id;
                uint16_t length;
                TunnelMessageType type;
                TunnelProtocol::parseHeader(header, conn_id, length, type);
                
                LOG_DEBUG << "Header: conn_id=" << conn_id 
                          << " length=" << length 
                          << " type=" << TunnelProtocol::messageTypeToString(type) << std::endl;
                
                // Validate message type
                if (!TunnelProtocol::isValidMessageType(static_cast<uint16_t>(type)))
                {
                    LOG_ERROR << "Invalid message type " << static_cast<uint16_t>(type) << std::endl;
                    _bytes_from_tunnel.clear();
                    break;
                }
                
                // Validate payload length
                if (length > TunnelProtocol::MAX_PAYLOAD_SIZE)
                {
                    LOG_ERROR << "Payload length " << length 
                              << " exceeds maximum " << TunnelProtocol::MAX_PAYLOAD_SIZE << std::endl;
                    _bytes_from_tunnel.clear();
                    break;
                }
                
                // Remove header from buffer
                _bytes_from_tunnel.erase(_bytes_from_tunnel.begin(), 
                                        _bytes_from_tunnel.begin() + TunnelProtocol::HEADER_SIZE);
                
                // Set up state for receiving payload
                _current_conn_id = conn_id;
                _current_type = type;
                _wait_for_payload = length;
                _wait_for_header = false;
                
                // If payload length is 0, message is complete immediately
                if (length == 0)
                {
                    LOG_INFO << "Zero-length payload, message complete" << std::endl;
                    _reconstructed_messages.emplace_back(conn_id, type, 0);
                    _wait_for_header = true;
                }
            }
            else
            {
                LOG_DEBUG << "Need more bytes for header (have " 
                     << _bytes_from_tunnel.size() << ")" << std::endl;
                break;
            }
        }
        else // Waiting for payload
        {
            if (_bytes_from_tunnel.size() >= _wait_for_payload)
            {
                LOG_DEBUG << " Have complete payload (" << _wait_for_payload << " bytes)" << std::endl;
                
                // Create message with payload
                TunnelMessage msg(_current_conn_id, _current_type, _wait_for_payload);
                memcpy(msg.payload.data(), _bytes_from_tunnel.data(), _wait_for_payload);
                
                // Remove payload from buffer
                _bytes_from_tunnel.erase(_bytes_from_tunnel.begin(),
                                        _bytes_from_tunnel.begin() + _wait_for_payload);
                
                // Add to queue
                _reconstructed_messages.push_back(std::move(msg));
                
                LOG_DEBUG << " Message complete and queued. Remaining bytes: " 
                          << _bytes_from_tunnel.size() << std::endl;
                
                // Reset state for next message
                _wait_for_header = true;
                _wait_for_payload = 0;
            }
            else
            {
                LOG_DEBUG << " Need " << _wait_for_payload 
                          << " bytes for payload, have " << _bytes_from_tunnel.size() << std::endl;
                break;
            }
        }
    }
    
    // Warn if queue is getting large
    if (_reconstructed_messages.size() > 10)
    {
        LOG_WARN << "Warning: Message queue depth is " 
                 << _reconstructed_messages.size() << std::endl;
    }
}


#pragma once

#include <vector>
#include <deque>
#include "tunnel_protocol.h"

// Represents a complete message received through the tunnel
struct TunnelMessage
{
    uint32_t conn_id;
    TunnelMessageType type;
    std::vector<char> payload;
    
    TunnelMessage(uint32_t id, TunnelMessageType t, size_t payload_size)
        : conn_id(id), type(t), payload(payload_size) {}
};

// Reconstructs tunnel messages from the TCP byte stream
class TunnelMessageReconstructor
{
private:
    // Bytes received from the tunnel that haven't been processed yet
    std::vector<char> _bytes_from_tunnel;
    
    // State machine for reconstruction

    /* The other side of the tunnel sends a 8-byte headers before sending data.
     * _wait_for_header is true while we are waiting for a length field.
     */
    bool _wait_for_header { true };

    uint32_t _current_conn_id { 0 };

    TunnelMessageType _current_type { TunnelMessageType::UDP_PACKET };

    /* If we are not waiting for a length field, _wait_for_data contains the number
     * of bytes that are still needed for the current payload.
     */
    size_t _wait_for_payload { 0 };


    // Queue of reconstructed messages ready to be processed
    std::deque<TunnelMessage> _reconstructed_messages;
    
    bool _verbose { true };
    
public:
    // Add bytes received from the tunnel to the reconstruction buffer
    void collect_from_tunnel(const char* buffer, int buflen);
    
    // Check if there are any complete messages ready
    inline bool hasMessages() const { return !_reconstructed_messages.empty(); }
    
    // Get the next complete message (check hasMessages() first!)
    inline TunnelMessage& frontMessage() { return _reconstructed_messages.front(); }
    
    // Remove the front message after processing
    inline void popMessage() { _reconstructed_messages.pop_front(); }
    
    // Get number of queued messages
    inline size_t messageCount() const { return _reconstructed_messages.size(); }
    
    // Control verbosity
    inline void setVerbose(bool onoff) { _verbose = onoff; }
};


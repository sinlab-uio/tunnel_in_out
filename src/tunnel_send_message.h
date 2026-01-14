#pragma once

#include <memory>

#include <unistd.h>

#include "tunnel_protocol.h"
#include "tcp.h"
#include "verbose.h"

bool sendTunnelMessage( const std::unique_ptr<TCPSocket>& tunnel, 
                        uint32_t conn_id,
                        TunnelMessageType type,
                        const char* payload,
                        uint16_t payload_len );


#pragma once

#include <memory>

#include "udp.h"
#include "tcp.h"
#include "sockaddr.h"

// Dispatch loop for TunnelClient
// Returns true if user requested quit (Q pressed)
// Returns false if connection was lost (should reconnect)
bool dispatch_loop( TCPSocket& tunnel,
                    UDPSocket& udp_forwarder,
                    std::shared_ptr<TCPSocket> webSock,
                    const SockAddr& dest_udp,
                    const SockAddr& dest_tcp );


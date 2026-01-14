#pragma once
#include <memory>

#include "udp.h"
#include "tcp.h"

void dispatch_loop( TCPSocket& tunnel_listener,
                    UDPSocket& outside_udp,
                    TCPSocket& outside_tcp_listener );


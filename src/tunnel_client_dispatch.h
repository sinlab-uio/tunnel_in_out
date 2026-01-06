#pragma once
// #include <iostream>
// #include <string>
// #include <vector>
// #include <algorithm>
#include <memory>

#include "udp.h"
#include "tcp.h"
#include "sockaddr.h"

void dispatch_loop( TCPSocket& tunnel,
                    UDPSocket& udp_forwarder,
                    std::shared_ptr<TCPSocket> webSock,
                    const SockAddr& dest_udp,
                    const SockAddr& dest_tcp );


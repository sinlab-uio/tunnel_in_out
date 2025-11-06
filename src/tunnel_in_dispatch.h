#pragma once
// #include <iostream>
// #include <string>
// #include <vector>
// #include <algorithm>
#include <memory>

#include "udp.h"
#include "tcp.h"

void dispatch_loop( TCPSocket& tunnel_listener,
                    UDPSocket& outside_udp,
                    TCPSocket& outside_tcp_listener,
                    std::shared_ptr<TCPSocket> tunnel,
                    std::shared_ptr<TCPSocket> webSock );


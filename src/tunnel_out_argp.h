#pragma once

#include <argp.h>

struct arguments
{
    std::string forward_host     {""};
    uint16_t    forward_tcp_port {0};
    uint16_t    forward_udp_port {0};

    std::string tunnel_host      {""};
    uint16_t    tunnel_port      {0};
};

void callArgParse( int argc, char* argv[], arguments& args );


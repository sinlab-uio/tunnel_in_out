#pragma once

#include <argp.h>

struct arguments
{
    std::string forward_udp_host {""};
    uint16_t    forward_udp_port {0};
    std::string forward_tcp_host {""};
    uint16_t    forward_tcp_port {0};

    std::string tunnel_host      {""};
    uint16_t    tunnel_port      {0};
    
    bool verbose {false};
};

void callArgParse( int argc, char* argv[], arguments& args );


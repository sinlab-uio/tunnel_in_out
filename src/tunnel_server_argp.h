#pragma once

#include <argp.h>

struct arguments
{
    uint16_t outside_udp {0};
    uint16_t outside_tcp {0};
    uint16_t tunnel_tcp  {0};
    bool verbose {false};
};

void callArgParse( int argc, char* argv[], arguments& args );


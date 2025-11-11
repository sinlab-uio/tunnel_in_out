#pragma once

#include <string>

#include <stdint.h>
#include <argp.h>

struct arguments
{
    std::string tunnel_host      {""};
    uint16_t    tunnel_port      {0};
    bool        send_too_long    {false};
};

void callArgParse( int argc, char* argv[], arguments& args );


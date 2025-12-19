#include <iostream>
#include <string>

#include <argp.h>
#include "generic_argp.h"
#include "outside_udp_argp.h"

const char *argp_program_version = "outside_udp 0.1";
const char *argp_program_bug_address = "griff@uio.no";
static char doc[] = "\n"
                    "OutsideUDP runs on the outside of a firewall. It sends dummy UDP packets to TunnelIn.\n"
                    "  <tunnel-url>\tthe hostname:port or 'dotted decimal address':port pointing to the TunnelIn machine.\n";
static char args_doc[] = "<tunnel-url>";
static struct argp_option options[] = {
    { "send-too-long", 'x', 0, 0, "Optional, attempt to send 10000-byte packet."},
    { 0 }
};

// #define CERR std::cerr << __FILE__ << ":" << __LINE__ << " "

static error_t parse_opt( int key, char *arg, struct argp_state *state )
{
    arguments *args = (arguments*)(state->input);

    switch( key )
    {
    case 'x' :
        args->send_too_long = true;
        break;
    case ARGP_KEY_ARG:
        switch( state->arg_num )
        {
        case 0:
            {
                args->tunnel_host = arg;
                int port = extractPort( args->tunnel_host );
                if( port < 0 )
                {
                    argp_error( state, "Positional option <tunnel-url> is missing or incorrect.");
                    return 0;
                }
                args->tunnel_port = port;
            }
            break;
        default :
            std::cerr << "Positional command line argument " << state->arg_num << " value " << arg << std::endl;
        }
        break;
    case ARGP_KEY_END:
        if (args->tunnel_host == "")
        {
            argp_error( state, "Positional option <tunnel-url> is missing.");
        }
        if (args->tunnel_port == 0)
        {
            argp_error( state, "Positional option <tunnel-url> is missing or incorrect.");
        }
        return 0;
    default:
        return 0;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };

void callArgParse( int argc, char* argv[], arguments& args )
{
    argp_parse( &argp, argc, argv, 0, 0, &args );
}


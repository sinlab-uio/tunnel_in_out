#include <iostream>
#include <string>

#include <argp.h>
#include "outside_udp_argp.h"

const char *argp_program_version = "outside_udp 0.1";
const char *argp_program_bug_address = "griff@uio.no";
static char doc[] = "\n"
                    "OutsideUDP runs on the outside of a firewall. It sends dummy UDP packets to TunnelIn.\n";
static char args_doc[] = " <tunnel-host> <tunnel-port>";
static struct argp_option options[] = {
    { "<tunnel-host>", 1, "string", OPTION_DOC, "The hostname or dotted decimal address of the TunnelIn machine."},
    { "<tunnel-port>", 2, "int",    OPTION_DOC, "TCP listening port of the TunnelIn process."},
    { "send-too-long", 'x', 0, 0, "Optional, attempt to send 10000-byte packet."},
    { 0 }
};

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
            args->tunnel_host = arg;
            break;
        case 1:
            args->tunnel_port = atoi(arg);
            break;
        default :
            std::cerr << "Positional command line argument " << state->arg_num << " value " << arg << std::endl;
        }
        break;
    case ARGP_KEY_END:
        if (args->tunnel_host == "")
        {
            argp_error( state, "Positional option <tunnel-host> is missing.");
        }
        if (args->tunnel_port == 0)
        {
            argp_error( state, "Positional option <tunnel-port> is missing.");
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


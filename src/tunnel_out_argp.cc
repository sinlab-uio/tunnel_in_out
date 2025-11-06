#include <iostream>
#include <string>

#include <argp.h>
#include "tunnel_out_argp.h"

const char *argp_program_version = "tunnel_out 0.1";
const char *argp_program_bug_address = "griff@uio.no";
static char doc[] = "\n"
                    "TunnelOut runs on the inside of a firewall. It connects actively to TunnelIn. "
                    "After that, it provides user-space splicing of a specified TCP connection and turns data arriving the tunnel back into UDP packets.\n";
static char args_doc[] = " <tunnel-host> <tunnel-port>";
static struct argp_option options[] = {
    { "<tunnel-host>",  1, "string", OPTION_DOC, "The hostname or dotted decimal address of the TunnelIn machine."},
    { "<tunnel-port>",  2, "int",    OPTION_DOC, "TCP listening port of the TunnelIn process."},
    { "fwd-host",     'f', "string", 0, "(mandatory) The hostname or dotted decimal address of the local machine."},
    { "fwd-udp",      'u', "int",    0, "(mandatory) The UDP port of the local machine."},
    { "fwd-tcp",      't', "int",    0, "(mandatory) The TCP listener port on the local machine."},
    { 0 }
};

static error_t parse_opt( int key, char *arg, struct argp_state *state )
{
    arguments *args = (arguments*)(state->input);

    switch( key )
    {
    case 'f':
        args->forward_host = arg;
        break;
    case 'u':
        args->forward_udp_port = atoi( arg );
        break;
    case 't':
        args->forward_tcp_port = atoi( arg );
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
        if (args->forward_host == "")
        {
            argp_error( state, "Mandatory option --fwd-host (-f) is missing.");
        }
        if (args->forward_udp_port == 0)
        {
            argp_error( state, "Mandatory option --fwd-udp (-u) is missing.");
        }
        if (args->forward_tcp_port == 0)
        {
            argp_error( state, "Mandatory option --fwd-tcp (-t) is missing.");
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


#include <iostream>
#include <string>

#include <argp.h>
#include <sys/types.h>
#include <stdint.h>
#include "tunnel_in_argp.h"

const char *argp_program_version = "tunnel_in 0.1";
const char *argp_program_bug_address = "griff@uio.no";
// static char args_doc[] = " <tunnel-port>"; // "[FILENAME]...";
static char doc[] = "\n"
                    "TunnelIn runs on the outside of a firewall. It waits passively for TunnelOut to connect to it. "
                    "After that, it will provide a user-space splicing of the specified TCP connections and a TCP tunnel for the specified UDP ports.\n";
static char* args_doc = doc;
static struct argp_option options[] = {
    { "<tunnel-port>",  1, "int", OPTION_DOC, "TCP listening port of this tunnel."},
    { "udp",          'u', "int", 0, "(mandatory) The UDP port to which TunnelIn will listen for packets from the outside."},
    { "tcp",          't', "int", 0, "(mandatory) The TCP port to which TunnelIn will listen for connection from the outside."},
    { 0 }
};

static error_t parse_opt( int key, char *arg, struct argp_state *state )
{
    arguments *args = (arguments*)(state->input);

    switch( key )
    {
    case 'u': args->outside_udp = atoi( arg ); break;
    case 't': args->outside_tcp = atoi( arg ); break;
    case ARGP_KEY_ARG:
        switch( state->arg_num )
        {
        case 0: args->tunnel_tcp = atoi(arg); break;
        default :
            std::cerr << "Positional command line argument " << state->arg_num << " value " << arg << std::endl;
        }
        break;
    case ARGP_KEY_END:
        if (args->outside_udp == 0)
        {
            argp_error( state, "Mandatory option --udp (-u) is missing.");
        }
        if (args->outside_tcp == 0)
        {
            argp_error( state, "Mandatory option --tcp (-t) is missing.");
        }
        if (args->tunnel_tcp == 0)
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


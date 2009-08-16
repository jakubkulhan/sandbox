#include <stdio.h>
#include "help.h"

void usage(const char *prog)
{
    printf("Usage: %s [ -h ] [ -f <forks> ] [ -r <root> ] [ -u <user> ] [ -g <group> ] [ -e ]\n"
           "    [ -0 <stdin> ] [ -1 <stdout> ] [ -2 <stderr> ]\n"
           "    [ <var>=<val>* ] -- <prog> [ <args> ]\n",
           prog);
}

void help(const char *prog)
{
    usage(prog);

    printf(
            "\n\n"
            "  -h              print this help\n"
            "  -f <forks>      create <forks> children\n"
            "  -r <root>       change root directory to <root>\n"
            "  -u <user>       change user to <user>\n"
            "  -g <group>      change group to <group>\n"
            "  -e              clear environment varuables\n"
            "  <var>=<val>     set environment variable <var> to <val>\n"
            "  -0 <stdin>      redirect stdin to <stdin>, possible formats:\n\n"
            "                   - :[user,group,mode]/foo/bar -- redirect to /foo/bar, chown to user:group and chmod mode\n\n"
            "                   - @[user,group,mode]/foo/bar -- redirect to UNIX domain socket /foo/bar\n\n"
            "                   - 127.0.0.1:666 -- listen at 127.0.0.1 and port 666 (IPv6 /e.g. [::1]:666) is supported\n\n"
            "  -1 <stdout>     redurect stdout to <stdout>, same possible formats as for -0\n"
            "  -2 <stderr>     redurect stderr to <stderr>, same possible formats as for -0\n"
    );
}

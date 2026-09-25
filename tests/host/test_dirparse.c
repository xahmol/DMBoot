/*
Host test: dirparse.c (directory lines, image names, dirtrace).
Reads commands on stdin, one per line, strings as hex (PETSCII bytes):
  P <hex>   parse a directory line -> "<rc> <type> <namehex> <diskidhex>"
  I <hex>   image kind of a name   -> "<kind>"
  F <hex>   does a name fit        -> "<0|1>"
  A <hex>   add to the trace       -> "<tracehex>"
  U         trace up               -> "<length> <tracehex>"
  C <0|1>   trace command          -> "<hex>"
  R         Ultimate path          -> "<hex>" (ASCII)
  Z         clear the trace        -> "ok"
Driven and checked by run_tests.py.
*/
#include <stdio.h>
#include <string.h>
#include "host.h"
#include "../../src/petconv.c"
#include "../../src/dirparse.c"

#define TRACE_SIZE 32               // Small, to test the bounds

static char trace[TRACE_SIZE];

static int unhex(const char *hex, char *out, int max)
{
    int n = 0;
    unsigned int byte;

    while (n < max && sscanf(hex + 2 * n, "%2x", &byte) == 1)
    {
        out[n++] = (char)byte;
    }
    return n;
}

static void puthex(const char *s, int n)
{
    if (!n)
    {
        printf("-");
    }
    for (int i = 0; i < n; i++)
    {
        printf("%02x", (unsigned char)s[i]);
    }
}

int main(void)
{
    char cmd[512];
    char arg[256];
    char out[256];

    while (fgets(cmd, sizeof(cmd), stdin))
    {
        int n = (strlen(cmd) > 2) ? unhex(cmd + 2, arg, sizeof(arg) - 1) : 0;
        arg[n] = 0;
        switch (cmd[0])
        {
        case 'P':
        {
            struct DirElement e;
            char diskid[DISK_ID_LEN + 1] = { 0 };
            char rc = dir_parse_line(arg, (char)n, &e, diskid);
            printf("%d %d ", rc, e.meta.type);
            puthex(e.name, strlen(e.name));
            printf(" ");
            puthex(diskid, strlen(diskid));
            printf(" %d\n", e.meta.length);
            break;
        }
        case 'I':
            printf("%d\n", dir_imagekind(arg));
            break;
        case 'F':
            printf("%d\n", trace_fits(trace, sizeof(trace), arg));
            break;
        case 'A':
            trace_add(trace, sizeof(trace), arg);
            puthex(trace, strlen(trace));
            printf("\n");
            break;
        case 'U':
        {
            unsigned len = trace_up(trace);
            printf("%u ", len);
            puthex(trace, strlen(trace));
            printf("\n");
            break;
        }
        case 'C':
            trace_command(out, sizeof(out), trace, cmd[2] == '1');
            puthex(out, strlen(out));
            printf("\n");
            break;
        case 'R':
            trace_ultpath(out, sizeof(out), trace);
            puthex(out, strlen(out));
            printf("\n");
            break;
        case 'Z':
            trace[0] = 0;
            printf("ok\n");
            break;
        }
    }
    return 0;
}

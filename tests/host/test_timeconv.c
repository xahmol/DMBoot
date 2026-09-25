/*
Host test: epoch_to_uiitime. Reads "epoch offset" lines on stdin, prints
"year month day hour minute second" (year as the Ultimate byte + 1900).
Driven and checked by run_tests.py.
*/
#include <stdio.h>
#include <string.h>
#include "host.h"
#include "../../src/timeconv.c"

int main(void)
{
    unsigned int epoch;
    int offset;
    char t[UII_TIME_BYTES];

    while (scanf("%u %d", &epoch, &offset) == 2)
    {
        epoch_to_uiitime(epoch, offset, t);
        printf("%d %d %d %d %d %d\n", 1900 + (unsigned char)t[0], t[1], t[2], t[3], t[4], t[5]);
    }
    return 0;
}

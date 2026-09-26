/*
DMBoot 128 v5 - REU access for the C128

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#include <c64/reu.h>
#include "reu128.h"

#pragma code(code)
#pragma data(data)
#pragma bss(bss)

#define REU_PROBE_VALUE     0x47
#define REU_MAX_PAGES       256

// ---------------------------------------------------------------------------
// Title:       REU probe barrier
// Description: Identity function that is never inlined. Passing the probed
//              byte through a real call boundary stops Oscar64 -O2 from
//              optimising the DMA read-back away.
// Syntax:      char reu128_barrier(char value);
// Input:       value - byte to pass through
// Output:      The same byte
// ---------------------------------------------------------------------------
__noinline char reu128_barrier(char value)
{
    return value;
}

// ---------------------------------------------------------------------------
// Title:       Slow down for DMA
// Description: Switches the C128 to 1 MHz for a REU transfer.
// Syntax:      char reu128_slow(void);
// Input:       None
// Output:      Previous value of the VIC-IIe clock register, to be passed to
//              reu128_restore_speed()
// ---------------------------------------------------------------------------
static inline char reu128_slow(void)
{
    volatile char *clock = (volatile char *)VIC_CLOCK_REG;
    char previous = *clock;
    *clock = previous & ~VIC_CLOCK_FAST;
    return previous;
}

// ---------------------------------------------------------------------------
// Title:       Restore speed after DMA
// Description: Restores the VIC-IIe clock register saved by reu128_slow().
// Syntax:      void reu128_restore_speed(char previous);
// Input:       previous - value returned by reu128_slow()
// Output:      None
// ---------------------------------------------------------------------------
static inline void reu128_restore_speed(char previous)
{
    *(volatile char *)VIC_CLOCK_REG = previous;
}

// ---------------------------------------------------------------------------
// Title:       Store to REU
// Description: Copies a block from C128 bank 0 memory to the REU at 1 MHz.
// Syntax:      __noinline void reu128_store(unsigned long raddr,
//                                const volatile char *src, unsigned length);
// Input:       raddr  - REU destination address
//              src    - bank 0 source address
//              length - number of bytes (1..65535)
// Output:      None
// ---------------------------------------------------------------------------
void reu128_store(unsigned long raddr, const volatile char *src, unsigned length)
{
    char speed = reu128_slow();
    reu_store(raddr, src, length);
    reu128_restore_speed(speed);
}

// ---------------------------------------------------------------------------
// Title:       Load from REU
// Description: Copies a block from the REU to C128 bank 0 memory at 1 MHz.
// Syntax:      void reu128_load(unsigned long raddr, volatile char *dst,
//                               unsigned length);
// Input:       raddr  - REU source address
//              dst    - bank 0 destination address
//              length - number of bytes (1..65535)
// Output:      None
// ---------------------------------------------------------------------------
__noinline void reu128_load(unsigned long raddr, volatile char *dst, unsigned length)
{
    char speed = reu128_slow();
    reu_load(raddr, dst, length);
    reu128_restore_speed(speed);
}

// ---------------------------------------------------------------------------
// Title:       Count REU pages
// Description: Detects the REU size by writing a marker to the first byte
//              of each 64 KB page and checking where it wraps around. The
//              test is destructive for the first byte of every page.
// Syntax:      unsigned reu128_count_pages(void);
// Input:       None
// Output:      Number of 64 KB pages (0 = no REU, 256 = 16 MB)
// ---------------------------------------------------------------------------
unsigned reu128_count_pages(void)
{
    volatile char marker;
    volatile char readback;
    char speed = reu128_slow();
    unsigned pages = REU_MAX_PAGES;

    // Oscar64's inline reu_store/reu_load here, not the __noinline
    // wrappers: with calls, Oscar64 1.32.273 -O2 (also the 2026-09-26
    // version) passed address 0 after a page address by clearing only
    // byte 3 of the address parameter; byte 2 kept the page, so the wrap
    // marker went to the probed page and every REU read as 64 KB
    marker = 0;
    reu_store(0, &marker, 1);
    reu_load(0, &readback, 1);
    if (reu128_barrier(readback) != 0)
    {
        pages = 0;
    }
    else
    {
        marker = REU_PROBE_VALUE;
        reu_store(0, &marker, 1);
        reu_load(0, &readback, 1);
        if (reu128_barrier(readback) != REU_PROBE_VALUE)
        {
            pages = 0;
        }
    }

    for (unsigned page = 1; pages == REU_MAX_PAGES && page < REU_MAX_PAGES; page++)
    {
        unsigned long address = (unsigned long)page << 16;

        marker = REU_PROBE_VALUE;
        reu_store(address, &marker, 1);
        marker = 0;
        reu_store(0, &marker, 1);

        reu_load(address, &readback, 1);
        if (reu128_barrier(readback) != REU_PROBE_VALUE)
        {
            pages = page;
        }
    }

    reu128_restore_speed(speed);
    return pages;
}

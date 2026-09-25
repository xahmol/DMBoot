/*
Shared VDCWin struct definition — included by vdc_win.h, vdcwin_nobnk.h,
and filebrowse.h so all three share a single, consistent type definition.
*/

#ifndef VDCWIN_TYPES_H
#define VDCWIN_TYPES_H

struct VDCWin
{
    char sx, sy, wx, wy;
    char cx, cy;
    unsigned sp, cp;
};

#endif

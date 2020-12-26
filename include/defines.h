/* -*- c-basic-offset: 2; tab-width: 2; indent-tabs-mode: nil -*-
 * vi: set shiftwidth=2 tabstop=2 expandtab:
 * :indentSize=2:tabSize=2:noTabs=true:
 */
/** @file
 * \date 10.01.2009
 * \author bader
 *
 * DraCopy (dc*) is a simple copy program.
 * DraBrowse (db*) is a simple file browser.
 *
 * Since both programs make use of kernal routines they shall
 * be able to work with most file oriented IEC devices.
 *
 * Created 2009 by Sascha Bader
 *
 * The code can be used freely as long as you retain
 * a notice describing original source and author.
 *
 * THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL,
 * BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!
 *
 * https://github.com/doj/dracopy
 */
#ifndef DEFINES_H_
#define DEFINES_H_

// Color scheme
#define DC_COLOR_BG COLOR_BLACK
#define DC_COLOR_BORDER COLOR_BLACK
#define DC_COLOR_TEXT COLOR_YELLOW
#define DC_COLOR_HIGHLIGHT COLOR_WHITE
#define DC_COLOR_DIM COLOR_GREEN
#define DC_COLOR_ERROR COLOR_RED
#define DC_COLOR_WARNING COLOR_VIOLET
#define DC_COLOR_EE COLOR_LIGHTBLUE
#define DC_COLOR_GRAY COLOR_GRAY2
#define DC_COLOR_GRAYBRIGHT COLOR_GRAY3
#define DC_COLOR_WAITKEY COLOR_GREEN

typedef unsigned char BYTE;

#define OK 0
#define ERROR -1
#define ABORT +1

#define BUFFERSIZE (4*254)

// height of sceen
#define SCREENH 25
// bottom row on screen
#define BOTTOM (SCREENH-1)

// height of menu frame
#define MENUH SCREENH
// y position of menu frame
#define MENUY 0

#define GETCWD dirs[context]
#define DIRW  25
#define DIR1X 0
#define DIR1Y 0

#define DIRH (context?DIR2H:DIR1H)
#define DIRX (context?DIR2X:DIR1X)
#define DIRY (context?DIR2Y:DIR1Y)

extern BYTE DIR1H;
extern BYTE DIR2H;
extern unsigned int SCREENW;
extern unsigned int MENUX;
extern unsigned int MENUXT;
extern unsigned int MENUW;
extern unsigned int DIR2X;
extern unsigned int DIR2Y;
extern char path[8][20];
extern BYTE pathdevice;
extern char pathfile[20];
extern BYTE pathrunboot;
extern char menupath[10][100];
extern char menuname[10][21];
extern char menufile[10][20];
extern unsigned int menurunboot[10];
extern unsigned int menudevice[10];
extern BYTE depth;
extern BYTE trace;

// keyboard buffer
#define KBCHARS 842
#define KBNUM 208

#define CH_LARROW 0x5f
#define CH_UARROW 0x5e
#define CH_POUND  0x5c

#pragma charmap (0xff, 0x5f);
#pragma charmap (0xfc, 0x5c);

#endif

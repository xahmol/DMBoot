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
#define DMB_COLOR_SELECT COLOR_CYAN
#define DMB_COLOR_HEADER1 COLOR_GREEN
#define DMB_COLOR_HEADER2 COLOR_LIGHTGREEN

// Command flag values
#define COMMAND_CMD             0x01
#define COMMAND_REU             0x02
#define COMMAND_IMGA            0x04
#define COMMAND_IMGB            0x08

// Execute flag values
#define EXEC_MOUNT              0x01
#define EXEC_FRC8               0x02
#define EXEC_RUN64              0x04
#define EXEC_FAST               0x08
#define EXEC_BOOT               0x10

// Config version
#define CFGVERSION              0x01

// VDC addresses
#define VDC16START              0x1000
#define VDC16END                0x1FFF
#define VDC64START              0x4000
#define VDC64END                0xFFFF

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

// Define highest device ID allowed
#define MAXDEVID 30

// Global variables
extern BYTE SCREENW;
extern BYTE DIRW;
extern BYTE MENUX; 
extern unsigned int validdriveid;
extern unsigned int idnr[30];
extern char path[8][20];
extern char pathfile[20];
extern BYTE pathdevice;
extern BYTE pathrunboot;
extern BYTE depth;
extern BYTE trace;
extern BYTE forceeight;
extern BYTE fastflag;
extern BYTE commandflag;
extern BYTE mountflag;
extern BYTE reuflag;
extern BYTE addmountflag;
extern BYTE runmountflag;
extern struct SlotStruct {
    char path[100];
    char menu[21];
    char file[20];
    char cmd[80];
    char reu_image[20];
    BYTE reusize;
    BYTE runboot;
    BYTE device;
    BYTE command;
    BYTE cfgvs;
    char image_a_path[100];
    char image_a_file[20];
    BYTE image_a_id;
    char image_b_path[100];
    char image_b_file[20];
    BYTE image_b_id;
};
extern struct SlotStruct Slot;
extern char newmenuname[36][21];
extern unsigned int newmenuoldslot[36];
extern BYTE bootdevice;
extern long secondsfromutc; 
extern unsigned char timeonflag;
extern char host[80];
extern char imagename[20];
extern char reufilepath[60];
extern char imageaname[20];
extern char imageapath[60];
extern unsigned char imageaid;
extern char imagebname[20];
extern char imagebpath[60];
extern unsigned char imagebid;
extern unsigned char reusize;
extern char* reusizelist[8];
extern unsigned char utilbuffer[328];
extern char configfilename[11];
extern char c128_ram;
extern unsigned char dm_apipresent;
extern unsigned int dm_apiversion;
extern unsigned char configversion;
extern unsigned char vdcmemory;
extern unsigned int vdc_alloc_address;

// keyboard buffer
#define KBCHARS 842
#define KBNUM 208

#define CH_LARROW 0x5f
#define CH_UARROW 0x5e
#define CH_POUND  0x5c

#pragma charmap (0xff, 0x5f);
#pragma charmap (0xfc, 0x5c);

#define BUILD_YEAR_CH0 (__DATE__[ 7])
#define BUILD_YEAR_CH1 (__DATE__[ 8])
#define BUILD_YEAR_CH2 (__DATE__[ 9])
#define BUILD_YEAR_CH3 (__DATE__[10])


#define BUILD_MONTH_IS_JAN (__DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_FEB (__DATE__[0] == 'F')
#define BUILD_MONTH_IS_MAR (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r')
#define BUILD_MONTH_IS_APR (__DATE__[0] == 'A' && __DATE__[1] == 'p')
#define BUILD_MONTH_IS_MAY (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y')
#define BUILD_MONTH_IS_JUN (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_JUL (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l')
#define BUILD_MONTH_IS_AUG (__DATE__[0] == 'A' && __DATE__[1] == 'u')
#define BUILD_MONTH_IS_SEP (__DATE__[0] == 'S')
#define BUILD_MONTH_IS_OCT (__DATE__[0] == 'O')
#define BUILD_MONTH_IS_NOV (__DATE__[0] == 'N')
#define BUILD_MONTH_IS_DEC (__DATE__[0] == 'D')


#define BUILD_MONTH_CH0 \
    ((BUILD_MONTH_IS_OCT || BUILD_MONTH_IS_NOV || BUILD_MONTH_IS_DEC) ? '1' : '0')

#define BUILD_MONTH_CH1 \
    ( \
        (BUILD_MONTH_IS_JAN) ? '1' : \
        (BUILD_MONTH_IS_FEB) ? '2' : \
        (BUILD_MONTH_IS_MAR) ? '3' : \
        (BUILD_MONTH_IS_APR) ? '4' : \
        (BUILD_MONTH_IS_MAY) ? '5' : \
        (BUILD_MONTH_IS_JUN) ? '6' : \
        (BUILD_MONTH_IS_JUL) ? '7' : \
        (BUILD_MONTH_IS_AUG) ? '8' : \
        (BUILD_MONTH_IS_SEP) ? '9' : \
        (BUILD_MONTH_IS_OCT) ? '0' : \
        (BUILD_MONTH_IS_NOV) ? '1' : \
        (BUILD_MONTH_IS_DEC) ? '2' : \
        /* error default */    '?' \
    )

#define BUILD_DAY_CH0 ((__DATE__[4] >= '0') ? (__DATE__[4]) : '0')
#define BUILD_DAY_CH1 (__DATE__[ 5])

#define BUILD_HOUR_CH0 (__TIME__[0])
#define BUILD_HOUR_CH1 (__TIME__[1])

#define BUILD_MIN_CH0 (__TIME__[3])
#define BUILD_MIN_CH1 (__TIME__[4])

#endif

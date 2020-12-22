//Includes
#include <stdio.h>
#include <string.h>
#include <peekpoke.h>
#include <cbm.h>
#include <conio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include "defines.h"
#include "ops.h"
#include "screen.h"
#include "version.h"
#include "base.h"
#include "cat.h"

#ifndef min
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif

//Functions
void checkdmdevices();
const char* deviceidtext (int id);
void std_write(unsigned char * file_name);
void std_read(unsigned char * file_name);
void mid(const char *src, size_t start, size_t length, char *dst, size_t dstlen);
char* pathconcat();

//Variables
BYTE DIR1H;
BYTE DIR2H;
unsigned int SCREENW;
unsigned int MENUX;
unsigned int MENUXT;
unsigned int MENUW;
unsigned int DIR2X;
unsigned int DIR2Y;
unsigned int validdriveid;
unsigned int idnr[30];
char path[8][20];
BYTE depth = 1;
BYTE trace = 0;
char menupath[10][100];
char menuname[10][20];
char menufile[10][20];
unsigned int menurunboot[10];
char spaces[81] = "                                                                                ";
char spacedest[81];
char data1[12] = "Test Data 1";
char data2[12] = "Test Data 2";
char data3[12] = "\0";
char data4[12] = "\0";

//Main program
int main() {
    int x;    
    
    //Check column width of present screen
    if ( PEEK(0xee) == 79) //Memory position $ee is present screen width
    {
        SCREENW = 80;  //Set flag for 80 column
        MENUX = 58; // x position of menu
        MENUXT = MENUX + 2; // x position of menu items
        MENUW = 15; // width of menu frame
        DIR2X = DIRW+4;
        DIR2Y = 0;
    }
    else
    {
        SCREENW = 40;  //Set flag for 40 column
        MENUX = 27; // x position of menu
        MENUXT = MENUX + 1; // x position of menu items
        MENUW = 13; // width of menu frame
        DIR2X = 0;
        DIR2Y = (DIR1Y+2+DIR1H);
    }
    
    initScreen(DC_COLOR_BORDER, DC_COLOR_BG, DC_COLOR_TEXT);

    gotoxy(0,0);
    mid(spaces,1,SCREENW,spacedest, sizeof(spacedest));
    revers(1);
    cprintf("%s",18,30,spacedest);
    gotoxy(0,0);  
    cprintf("DMBoot 128: Welcome to your C128.\n\n\r",18,30);
    revers(0);

    textcolor(DC_COLOR_TEXT);

    cputs("Hello, World!\n\r");
    cprintf("Screenwidth = %u\n\r", SCREENW);
    checkdmdevices();
    waitKey(0);

    mainLoopBrowse();

    clrscr();
    cputs("Trace test\n\r");
    cprintf("Trace: %u Depth: %u\n\r",trace,depth);
    for (x = 1; x < depth ; ++x )
    {
        cprintf("%u: %s\n\r",x,path[x]);
    }
    cputs("\n\n\rFull path:\n\r");
    cprintf("%s\n\r", pathconcat());
    waitKey(0);

    clrscr();
    cputs("File operations test\n\r");

    // File read & write with stdio functions
    cputs("Writing with stido.h\n\r");
    std_write("testfile");
    cputs("\n\rReading with stido.h\n\r");
    std_read("testfile");
    cprintf("\n\rdata3 : %s\n\r",data3);
    cprintf("data4 : %s\n\r",data4);
    cprintf("\n\r");

    waitKey(0);
   
    exitScreen();
    return 0;
}

//User defined functions

void checkdmdevices() {
//Read memory for devices recognised by Device Manager Rom
    unsigned int checksum = 0x42; // Set base value for checksum
    unsigned int x;

    for (x=0; x<30; ++x) // Check for device number 0 to 30
    {
        idnr[x] = PEEK(0x0c00 + x);
        checksum = checksum ^ idnr[x]; // Perform bitwise exlusive OR with checksum for each memory position
    }

    if (checksum == PEEK(0x0c00+31) ) // Compare calculated checsum with memory position where valid checksum would be
    {
        validdriveid = 1;
        printf("Checksum is correct!\n");
        for (x=8; x<30; ++x)
        {
            if (idnr[x] != 0)
            {
                printf("ID %u = %s\n",x,deviceidtext(idnr[x]));
            }
        }
    }
    else
    {
        validdriveid = 0;
        printf("Checksum is not correct.\n");
    }   
}

const char* deviceidtext (int id)
{
    switch( id )
    {
        case 0:
            return "none";
        case 1:
            return "unkown";
        case 2:
            return "U2 A";
        case 3:
            return "U2 B";
        case 4:
            return "SD2IEC";
        case 5:
            return "uIEC";
        case 6:
            return "Printer";
        case 7:
            return "Plotter";
        case 8:
            return "SoftIEC";
        case 40:
            return "1540";
        case 41:
            return "1541";
        case 51:
            return "1551";
        case 70:
            return "1570";
        case 71:
            return "1571";
        case 81:
            return "1581";
        default:
            return "other";
    }
}

void std_write(unsigned char * file_name)
{
    FILE *file;
    unsigned char n;
    cputs("Opening data file...\n\r");
    _filetype = 's';
    if(file = fopen(file_name, "w"))
        {
            cputs("Writing...\n\r");
            n = fwrite(data1, sizeof(unsigned char)*11, 1, file);
            n = n + fwrite(data2, sizeof(unsigned char)*11, 1, file);
            if(n != 2)
            {
                cputs("Error: File could not be written.\n\r");
                fclose(file);
            }
            else
            {
                cputs("Done.\n\r");
                fclose(file);       
            }
        }
    else
        {
            cputs("File could not be opened\n\r");
        }
}

void std_read(unsigned char * file_name)
{
    FILE *file;
    unsigned char n;
    cputs("Opening data file...\n\r");
    _filetype = 's';
    if(file = fopen(file_name, "r"))
    {
        cputs("Reading...\n\r");
        n = fread(data3, sizeof(unsigned char)*11, 1, file);
        n = n + fread(data4, sizeof(unsigned char)*11, 1, file);
        if(n != 2)
        {
            cputs("Error while reading!\n\r");
            fclose(file);
        }
        else
        {
            cprintf("Done.\n\r");
            fclose(file);
        }
    }
    else
    {
        cputs("File could not be opened\n\r");
    }
}

void mid(const char *src, size_t start, size_t length, char *dst, size_t dstlen)
{       size_t len = min( dstlen - 1, length);
 
        strncpy(dst, src + start, len);
        // zero terminate because strncpy() didn't ? 
        if(len < length)
                dst[dstlen-1] = 0;
}

char* pathconcat()
{
    char concat[100] ="";
    int x;

    for (x=1 ; x < depth ; ++x)
    {
        strcat( concat, path[x] );
        strcat( concat, "/");
    }
    return concat;
}
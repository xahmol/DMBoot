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
char getkey(BYTE mask);
void pickmenuslot();

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
char pathfile[20];
BYTE pathdevice;
BYTE pathrunboot;
BYTE depth = 1;
BYTE trace = 0;
char menupath[10][100];
char menuname[10][21];
char menufile[10][20];
unsigned int menurunboot[10];
unsigned int menudevice[10];
char spaces[81]    = "                                                                                ";
char underline[81] = "________________________________________________________________________________";
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
    cprintf("%s",spacedest);
    gotoxy(0,0);  
    cprintf("DMBoot 128: Welcome to your C128.\n\n\r");
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
    cprintf("Filename: %s\n\r",pathfile);
    cprintf("Device: %i\n\r",pathdevice);
    cprintf("Run/boot flag: %i\n\r", pathrunboot);
    waitKey(0);

    if (trace == 1)
    {
        pickmenuslot();
    }

    clrscr();
    cputs("Present menu slots:\n\n\r");
    for ( x=1 ; x<11 ; ++x )
    {
        revers(1);
        cprintf(" %i ",x-1);
        revers(0);
        if ( strlen(menuname[x]) == 0 )
        {
            cputs(" <EMPTY>\n\r");
        }
        else
        {
            cprintf(" %s\n\r",menuname[x]);
        }
    }
    waitKey(0);

    //clrscr();
    //cputs("File operations test\n\r");

    // File read & write with stdio functions
    //cputs("Writing with stido.h\n\r");
    //std_write("testfile");
    //cputs("\n\rReading with stido.h\n\r");
    //std_read("testfile");
    //cprintf("\n\rdata3 : %s\n\r",data3);
    //cprintf("data4 : %s\n\r",data4);
    //cprintf("\n\r");

    //waitKey(0);
   
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
    // Function to return device ID string based on ID value

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
{       
    // Function to provide MID$ equivalent

    size_t len = min( dstlen - 1, length);
 
    strncpy(dst, src + start, len);
    // zero terminate because strncpy() didn't ? 
    if(len < length)
        dst[dstlen-1] = 0;
}

char* pathconcat()
{
    // Function to concatinate the path strings

    char concat[100] ="";
    int x;

    if ( devicetype[pathdevice] == VICE || devicetype[pathdevice] == U64)
    {
        strcat( concat, "cd:/");
    }
    else
    {
        strcat( concat, "cd/");
    }
    for (x=1 ; x < depth ; ++x)
    {
        strcat( concat, path[x] );
        strcat( concat, "/");
    }
    return concat;
}

char getkey(BYTE mask)
{
    // Funnction to wait for key within input validation mask
    // Mask values for input validation (adds up for combinations):
    // 00000001 =   1 = Numeric
    // 00000010 =   2 = Alpha lowercase
    // 00000100 =   4 = Alpha uppercase
    // 00001000 =   8 = Up and down
    // 00010000 =  16 = Left and right
    // 00100000 =  32 = Delete and insert
    // 01000000 =  64 = Return
    // 10000000 = 128 = Y and N

    BYTE keychar;

    do
    {
        keychar = cgetc();
    } while ( !(mask&1 && keychar > 47 && keychar < 58) && !(mask&2 && keychar > 31 && keychar < 96) && !(mask&4 && keychar > 95 && keychar < 128) && !(mask&8 && (keychar == 29 || keychar == 157)) && !(mask&16 && (keychar == 17 || keychar == 145)) && !(mask&32 && (keychar == 20 || keychar == 148)) && !(mask&64 && keychar == 13) && !(mask&128 && (keychar == 78 || keychar == 89)) );
    return keychar;    
}

void pickmenuslot()
{
    int x;
    int menuslot;
    BYTE yesno;
    BYTE selected = 0;
    
    clrscr();
    cputs("Present menu slots:\n\n\r");
    for ( x=1 ; x<11 ; ++x )
    {
        revers(1);
        cprintf(" %i ",x-1);
        revers(0);
        if ( strlen(menuname[x]) == 0 )
        {
            cputs(" <EMPTY>\n\r");
        }
        else
        {
            cprintf(" %s\n\r",menuname[x]);
        }
    }
    cputs("\nChoose slot by pressing number: ");
    menuslot = getkey(1) - 48;
    selected = 1 ;
    cprintf("%i\n\r", menuslot);
    if ( strlen(menuname[menuslot+1]) != 0 )
    {
        cprintf("Slot not empty. Are you sure? Y/N ");
        yesno = getkey(128);
        cprintf("%c\n\r", yesno);
        if ( yesno = 78 )
        {
            selected = 0;
        }
    }
    if ( selected == 1)
    {
        gotoxy(0,15);
        cputs("Choose name for slot:");
        strcpy(menuname[menuslot],pathfile);
        textInput(0,16,menuname[menuslot],20);
        menudevice[menuslot] = pathdevice;
        strcpy(menufile[menuslot],pathfile);
        strcpy(menupath[menuslot],pathconcat());
        menurunboot[menuslot] = pathrunboot;
    }
}
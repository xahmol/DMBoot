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

//Functions
void checkdmdevices();
const char* deviceidtext (int id);
void std_write(unsigned char * file_name);
void std_read(unsigned char * file_name);

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
char spaces[81] = "                                                                                ";
char data1[12] = "Test Data 1";
char data2[12] = "Test Data 2";
char data3[12] = "\0";
char data4[12] = "\0";

//Main program
int main() {

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

    printf("%c%cDMBoot 128: Welcome to your C128.%s\n\n",18,30);

    textcolor(DC_COLOR_TEXT);

    printf("Hello, World!\n");
    printf("Screenwidth = %u\n", SCREENW);
    checkdmdevices();
    waitKey(0);

    mainLoopBrowse();

    clrscr();
    printf("File operations test");

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
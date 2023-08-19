// ====================================================================================
// vdc_core.c
// Functions and definitions which make working with the Commodore 128's VDC easier
//
// Credits for code and inspiration:
//
// C128 Programmers Reference Guide:
// http://www.zimmers.net/anonftp/pub/cbm/manuals/c128/C128_Programmers_Reference_Guide.pdf
//
// Scott Hutter - VDC Core functions inspiration:
// https://github.com/Commodore64128/vdc_gui/blob/master/src/vdc_core.c
// (used as starting point, but changed to inline assembler for core functions, added VDC wait statements and expanded)
//
// Francesco Sblendorio - Screen Utility:
// https://github.com/xlar54/ultimateii-dos-lib/blob/master/src/samples/screen_utility.c
//
// DevDef: Commodore 128 Assembly - Part 3: The 80-column (8563) chip
// https://devdef.blogspot.com/2018/03/commodore-128-assembly-part-3-80-column.html
//
// Tips and Tricks for C128: VDC
// http://commodore128.mirkosoft.sk/vdc.html
//
// 6502.org: Practical Memory Move Routines
// http://6502.org/source/general/memory_move.html
//
// =====================================================================================

#include <stdio.h>
#include <string.h>
#include <peekpoke.h>
#include <cbm.h>
#include <conio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <device.h>
#include <accelerator.h>
#include <c128.h>
#include "defines.h"
#include "vdc.h"

unsigned char VDC_ReadRegister(unsigned char registeraddress)
{
	// Function to read a VDC register
	// Input: Registernumber, Output: returned register value

	VDC_regadd = registeraddress;
	
	VDC_ReadRegister_core();

	return VDC_regval;						// Return the register value
}

void VDC_WriteRegister(unsigned char registeraddress, unsigned char registervalue)
{
	// Function to write a VDC register
	// Input: Registernumber and value to write

	VDC_regadd = registeraddress;
	VDC_regval = registervalue;

	VDC_WriteRegister_core();
}

void VDC_Poke(int address,  unsigned char value)
{
	// Function to store a value to a VDC address
	// Innput: VDC address and value to store
	VDC_addrh = (address>>8) & 0xff;
	VDC_addrl = address & 0xff;
	VDC_value = value;

	VDC_Poke_core();
}

unsigned char VDC_Peek(int address)
{
	// Function to read a value from a VDC address
	// Innput: VDC address, Output: read value

	VDC_addrh = (address>>8) & 0xff;
	VDC_addrl = address & 0xff;

	VDC_Peek_core();

	/* Return value via VDC register 31 */
	return VDC_value;
}

unsigned char VDC_DetectVDCMemSize()
{
	// Function to detect the VDC memory size
	// Output: memorysize 16 or 64

	VDC_DetectVDCMemSize_core();
	return VDC_value;
}

void VDC_MemCopy(unsigned int sourceaddr, unsigned int destaddr, unsigned int length)
{
	// Function to copy memory from one to another position within VDC memory
	// Input: Sourceaddress, destination address, number of bytes to copy

	VDC_addrh = (sourceaddr>>8) & 0xff;		// Obtain high byte of source address
	VDC_addrl = sourceaddr & 0xff;			// Obtain low byte of source address
	VDC_desth = (destaddr>>8) & 0xff;		// Obtain high byte of destination address
	VDC_destl = destaddr & 0xff;			// Obtain low byte of destination address
	VDC_tmp1 = ((length>>8) & 0xff) + 1;	// Obtain number of 256 byte pages to copy
	VDC_tmp2 = length & 0xff;				// Obtain length in last page to copy

	VDC_MemCopy_core();
}

unsigned int VDC_RowColToAddress(unsigned char row, unsigned char col)
{
	/* Function returns a VDC memory address for a given row and column */

	unsigned int addr;
	addr = row * 80 + col;

	if (addr < 2000)
		return addr;
	else
		return -1;
}

void VDC_HChar(unsigned char row, unsigned char col, unsigned char character, unsigned char length, unsigned char attribute)
{
	// Function to draw horizontal line with given character (draws from left to right)
	// Input: row and column of start position (left end of line), screencode of character to draw line with,
	//		  length in number of character positions, attribute color value

	unsigned int startaddress = VDC_RowColToAddress(row,col);
	VDC_addrh = (startaddress>>8) & 0xff;	// Obtain high byte of start address
	VDC_addrl = startaddress & 0xff;		// Obtain low byte of start address
	VDC_tmp1 = character;					// Obtain character value
	VDC_tmp2 = length - 1;					// Obtain length value
	VDC_tmp3 = attribute;					// Ontain attribute value

	VDC_HChar_core();
}

void VDC_VChar(unsigned char row, unsigned char col, unsigned char character, unsigned char length, unsigned char attribute)
{
	// Function to draw vertical line with given character (draws from top to bottom)
	// Input: row and column of start position (top end of line), screencode of character to draw line with,
	//		  length in number of character positions, attribute color value

	unsigned int startaddress = VDC_RowColToAddress(row,col);
	VDC_addrh = (startaddress>>8) & 0xff;	// Obtain high byte of start address
	VDC_addrl = startaddress & 0xff;		// Obtain low byte of start address
	VDC_tmp1 = character;					// Obtain character value
	VDC_tmp2 = length;						// Obtain length value
	VDC_tmp3 = attribute;					// Ontain attribute value

	VDC_VChar_core();
}

void VDC_CopyMemToVDC(unsigned int vdcAddress, unsigned int memAddress, unsigned int length)
{
	// Function to copy memory from VDC memory to standard memory
	// Input: Source VDC address, destination standard memory address and bank, number of bytes to copy

	length--;

	VDC_addrh = (memAddress>>8) & 0xff;					// Obtain high byte of source address
	VDC_addrl = memAddress & 0xff;						// Obtain low byte of source address
	VDC_desth = (vdcAddress>>8) & 0xff;					// Obtain high byte of destination address
	VDC_destl = vdcAddress & 0xff;						// Obtain low byte of destination address
	VDC_tmp1 = ((length>>8) & 0xff);					// Obtain number of 256 byte pages to copy
	VDC_tmp2 = length & 0xff;							// Obtain length in last page to copy

	VDC_CopyMemToVDC_core();
}

void VDC_CopyVDCToMem(unsigned int vdcAddress, unsigned int memAddress, unsigned int length)
{
	// Function to copy memory from VDC memory to standard memory
	// Input: Source VDC address, destination standard memory address and bank, number of bytes to copy

	length--;

	VDC_addrh = (vdcAddress>>8) & 0xff;					// Obtain high byte of source VDC address
	VDC_addrl = vdcAddress & 0xff;						// Obtain low byte of source VDC address
	VDC_desth = (memAddress>>8) & 0xff;					// Obtain high byte of destination address
	VDC_destl = memAddress & 0xff;						// Obtain low byte of destination address
	VDC_tmp1 = ((length>>8) & 0xff);					// Obtain number of 256 byte pages to copy
	VDC_tmp2 = length & 0xff;							// Obtain length in last page to copy

	VDC_CopyVDCToMem_core();
}

void VDC_FillArea(unsigned char row, unsigned char col, unsigned char character, unsigned char length, unsigned char height, unsigned char attribute)
{
	// Function to draw area with given character (draws from topleft to bottomright)
	// Input: row and column of start position (topleft), screencode of character to draw line with,
	//		  length and height in number of character positions, attribute color value

	unsigned int startaddress = VDC_RowColToAddress(row,col);
	VDC_addrh = (startaddress>>8) & 0xff;	// Obtain high byte of start address
	VDC_addrl = startaddress & 0xff;		// Obtain low byte of start address
	VDC_tmp1 = character;					// Obtain character value
	VDC_tmp2 = length - 1;					// Obtain length value
	VDC_tmp3 = attribute;					// Ontain attribute value
	VDC_tmp4 = height;						// Obtain number of lines

	VDC_FillArea_core();
}
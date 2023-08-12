// ====================================================================================
// vdc_core.h
//
// Functions and definitions which make working with the Commodore 128's VDC easier
//
// Code is released under the GPL
// Scott Hutter - 2010
//
// =====================================================================================

#ifndef _VDC_
#define _VDC_

// VDC addressing
#define VDCBASETEXT         0x0000      // Base address for text screen characters
#define VDCBASEATTR         0x0800      // Base address for text screen attributes
#define VDCSWAPTEXT         0x1000      // Base address for swap text screen characters
#define VDCSWAPATTR         0x1800      // Base address for swap text screen attributes
#define VDCCHARSTD          0x2000      // Base address for standard charset
#define VDCCHARALT          0x3000      // Base address for alternate charset
#define VDCEXTENDED         0x4000      // Base address of 64K VDC extended memory space

// VDC color values
#define VDC_BLACK	0
#define VDC_DGREY	1
#define VDC_DBLUE	2
#define VDC_LBLUE	3
#define VDC_DGREEN	4
#define VDC_LGREEN	5
#define VDC_DCYAN	6
#define VDC_LCYAN	7
#define VDC_DRED	8
#define VDC_LRED	9
#define VDC_DPURPLE	10
#define VDC_LPURPLE	11
#define VDC_DYELLOW	12
#define VDC_LYELLOW	13
#define VDC_LGREY	14
#define VDC_WHITE	15

#define VDC_CURSORMODE_SOLID      0
#define VDC_CURSORMODE_NONE       1
#define VDC_CURSORMODE_FAST       2
#define VDC_CURSORMODE_NORMAL     3

#define VDC_A_BLINK              16
#define VDC_A_UNDERLINE          32
#define VDC_A_REVERSE            64
#define VDC_A_ALTCHAR           128

// Variables in core Functions
extern unsigned char VDC_regadd;
extern unsigned char VDC_regval;
extern unsigned char VDC_addrh;
extern unsigned char VDC_addrl;
extern unsigned char VDC_desth;
extern unsigned char VDC_destl;
extern unsigned char VDC_strideh;
extern unsigned char VDC_stridel;
extern unsigned char VDC_value;
extern unsigned char VDC_tmp1;
extern unsigned char VDC_tmp2;
extern unsigned char VDC_tmp3;
extern unsigned char VDC_tmp4;

// Import assembly core Functions
void VDC_ReadRegister_core();
void VDC_WriteRegister_core();
void VDC_Poke_core();
void VDC_Peek_core();
void VDC_DetectVDCMemSize_core();
void VDC_SetExtendedVDCMemSize();
void VDC_CopyCharsetsfromROM();
void VDC_MemCopy_core();
void VDC_HChar_core();
void VDC_VChar_core();
void VDC_CopyMemToVDC_core();
void VDC_CopyVDCToMem_core();
void VDC_FillArea_core();

// Function Prototypes
unsigned char VDC_ReadRegister(unsigned char registeraddress);
void VDC_WriteRegister(unsigned char registeraddress, unsigned char registervalue);
unsigned char VDC_DetectVDCMemSize();
void VDC_Poke(int address,  unsigned char value);
unsigned char VDC_Peek(int address);
unsigned char VDC_DetectVDCMemSize();
void VDC_SetExtendedVDCMemSize();
void VDC_MemCopy(unsigned int sourceaddr, unsigned int destaddr, unsigned int length);
unsigned int VDC_RowColToAddress(unsigned char row, unsigned char col);
void VDC_HChar(unsigned char row, unsigned char col, unsigned char character, unsigned char length, unsigned char attribute);
void VDC_VChar(unsigned char row, unsigned char col, unsigned char character, unsigned char length, unsigned char attribute);
void VDC_CopyMemToVDC(unsigned int vdcAddress, unsigned int memAddress, unsigned int length);
void VDC_CopyVDCToMem(unsigned int vdcAddress, unsigned int memAddress, unsigned int length);
void VDC_FillArea(unsigned char row, unsigned char col, unsigned char character, unsigned char length, unsigned char height, unsigned char attribute);

#endif
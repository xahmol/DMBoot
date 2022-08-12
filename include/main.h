#ifndef MAIN_H_
#define MAIN_H_

void std_write(char * file_name);
void std_read(char * file_name);
void mid(const char *src, size_t start, size_t length, char *dst, size_t dstlen);
void cspaces(unsigned char number);
char* pathconcat();
char getkey(BYTE mask);
unsigned char dm_getdevicetype(unsigned char id);
void dm_sethsidviaapi();
//void checkdmdevices();
//const char* deviceidtext (int id);

#endif
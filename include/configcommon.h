#ifndef CONFIGCOMMON_H_
#define CONFIGCOMMON_H_

#ifndef min
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif

extern long secondsfromutc; 
extern unsigned char timeonflag;
extern char host[80];
extern char reufilename[20];
extern char reufilepath[60];
extern char imageaname[20];
extern char imageapath[60];
extern unsigned char imageaid;
extern char imagebname[20];
extern char imagebpath[60];
extern unsigned char imagebid;
extern unsigned char reusize;
extern char* reusizelist[8];
extern unsigned char buffer[328];

void mid(const char *src, size_t start, size_t length, char *dst, size_t dstlen);
BYTE dosCommand(const BYTE lfn, const BYTE drive, const BYTE sec_addr, const char *cmd);
int cmd(const BYTE device, const char *cmd);
void writeconfigfile(char* filename);
void readconfigfile(char* filename);

#endif /* __CONFIGCOMMON_H__ */
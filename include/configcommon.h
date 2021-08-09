#ifndef CONFIGCOMMON_H_
#define CONFIGCOMMON_H_

#ifndef min
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif

void writeconfigfile(char* filename);
void readconfigfile(char* filename);

#endif /* __CONFIGCOMMON_H__ */
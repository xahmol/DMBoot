#ifndef __DMAPI_H__
#define __DMAPI_H__

extern void dm_getapiversion_core();
extern void dm_getdevicetype_core();
extern void dm_sethsidviaapi();
extern void dm_gethsidviaapi();

extern void* dm_run64;

extern unsigned char dm_apipresent;
extern unsigned char dm_apiverhigb;
extern unsigned char dm_apiverlowb;
extern unsigned char dm_devtype;
extern char dm_prgnam[20];
extern unsigned char dm_prglen;
extern unsigned char dm_devid;

#endif /* __DMAPI_H__ */
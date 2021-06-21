#ifndef __GEOSRAMBOOT_H__
#define __GEOSRAMBOOT_H__

extern void startgeos(void);
extern unsigned char errorcode;

void bootgeos()
{
    startgeos();
    switch (errorcode)
	{
	case 1:
		printf("REU not detected.");
		break;

    case 2:
		printf("Not a valid GEOS RAMboot image.");
		break;
	
	default:
		break;
	}
}

#endif /* __GEOSRAMBOOT_H__ */
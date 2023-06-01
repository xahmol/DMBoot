// Functions to set time using NTP server
// Source: https://github.com/MaxPlap/ntp2ultimate

#define NTP_TIMESTAMP_DELTA 2208988800ul //0x83AA7E80
#define cia_hour            (*(char*)0xDC0B)                
#define cia_minutes         (*(char*)0xDC0A)
#define cia_seconds         (*(char*)0xDC09)
#define cia_tensofsec       (*(char*)0xDC08)

unsigned char CheckStatus();
void NTP_time_set(signed char hoursfromutc);
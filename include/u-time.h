// Functions to set time using NTP server
// Source: https://github.com/MaxPlap/ntp2ultimate

#define NTP_TIMESTAMP_DELTA 2208988800ul //0x83AA7E80

unsigned char CheckStatus();
void NTP_time_set(signed char hoursfromutc);
/*
DMBoot 128 v5 - ASCII / PETSCII conversion

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
*/

#ifndef PETCONV_H
#define PETCONV_H

void asc2pet(char *dst, const char *src, unsigned dstsize);
void pet2asc(char *dst, const char *src, unsigned size);

#pragma compile("petconv.c")

#endif // PETCONV_H

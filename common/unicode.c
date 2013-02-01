/*
 * unicode.c
 * Copyright (C) 2013 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

// #include <iconv.h>
// #include <stdlib.h>
/*----------------------------------------------------------------------------*/
#include "unicode.h"
/*----------------------------------------------------------------------------*/
#undef DEBUG
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
#include <stdio.h>
#endif
/*----------------------------------------------------------------------------*/
// void encode(char *inbuf, unsigned int length)
// {
//   size_t insize = length * 2, outsize = length * 4;
//   char *outptr, *outbuf;
//   iconv_t cd;
//   size_t nconv;
// //   uint16_t i;
// 
//   cd = iconv_open("UTF-8", "UTF-16LE");
//   if (cd == (iconv_t)-1)
//   {
//     printf("ioconv initialization error\n");
//     return;
//   }
// 
//   outbuf = malloc(outsize);
//   outptr = outbuf;
//   nconv = iconv(cd, &inbuf, &insize, &outptr, &outsize);
//   *outptr = 0;
//   if (iconv_close (cd) != 0)
//     printf("iconv_close");
// 
// //   //Dump name
// //   printf("UTF-8 name: ");
// //   for (i = 0; i < outsize; i++)
// //     printf("%02X ", (unsigned char)outbuf[i]);
//   printf("STR: %s\n", outbuf);
//   free(outbuf);
// }
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
static void dumpChar16String(char16_t *str)
{
  while (*str)
    printf("%04X ", (unsigned int)(*str++) & 0xFFFF);
  printf("\n");
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
static void dumpChar8String(char *str)
{
  while (*str)
    printf("%02X ", (unsigned int)(*str++) & 0xFF);
  printf("\n");
}
#endif
/*----------------------------------------------------------------------------*/
/* TODO Add surrogate pairs support */
/* Convert the string from UTF-16LE terminated with 0 to UTF-8 string */
uint16_t uFromUtf16(char *dest, const char16_t *src, uint16_t maxLength)
{
  char16_t value;
  char *ptr = dest;

#ifdef DEBUG
  printf("Src string: ");
  dumpChar16String(src);
#endif

  while ((value = *src++) && ptr - dest < maxLength - 1)
  {
    if (value <= 0x007F)
    {
      *ptr++ = (char)value;
    }
    else if (value >= 0x0080 && value <= 0x07FF)
    {
      *ptr++ = (char)(0xC0 | (value >> 6));
      *ptr++ = (char)(0x80 | (value & 0x003F));
    }
    else
    {
      *ptr++ = (char)(0xE0 | (value >> 12));
      *ptr++ = (char)(0x80 | ((value >> 6) & 0x003F));
      *ptr++ = (char)(0x80 | (value & 0x003F));
    }
  }
  *ptr = '\0';

#ifdef DEBUG
  printf("Dest string: ");
  dumpChar8String(dest);
#endif

  return (uint16_t)(ptr - dest);
}
/*----------------------------------------------------------------------------*/
/* TODO Add surrogate pairs support */
/* Convert the string from UTF-8 terminated with 0 to UTF-16LE string */
uint16_t uToUtf16(char16_t *dest, const char *src, uint16_t maxLength)
{
  uint16_t count = 0;
  char16_t code;
  uint8_t value;

  while ((value = *src++) && count < maxLength - 1)
  {
    if (!(value & 0x80)) /* U+007F */
    {
      *dest++ = value;
      count++;
    }
    if ((value & 0xE0) == 0xC0) /* U+07FF */
    {
      code = (char16_t)(value & 0x1F) << 6;
      value = *src++;
      if ((value & 0xC0) == 0x80)
      {
        code |= (char16_t)(value & 0x3F);
        *dest++ = value;
        count++;
      }
    }
    if ((value & 0xF0) == 0xE0) /* U+FFFF */
    {
      code = (char16_t)(value & 0x0F) << 12;
      value = *src++;
      if ((value & 0xC0) == 0x80)
      {
        code |= (char16_t)(value & 0x3F) << 6;
        value = *src++;
        if ((value & 0xC0) == 0x80)
        {
          code |= (char16_t)(value & 0x3F);
          *dest++ = value;
          count++;
        }
      }
    }
  }
  *dest = '\0';
  return count;
}

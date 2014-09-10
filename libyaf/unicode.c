/*
 * unicode.c
 * Copyright (C) 2013 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <memory.h>
#include <libyaf/unicode.h>
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_UNICODE_DEBUG
#include <stdio.h>
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_UNICODE_DEBUG
static void dumpChar16String(char16_t *str)
{
  while (*str)
    printf("%04X ", (unsigned int)(*str++) & 0xFFFF);
  printf("\n");
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_UNICODE_DEBUG
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

#ifdef CONFIG_UNICODE_DEBUG
  printf("Src string: ");
  dumpChar16String(src);
#endif

  while ((value = *src++) && ptr - dest < maxLength - 1)
  {
    value = fromLittleEndian16(value);

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

#ifdef CONFIG_UNICODE_DEBUG
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
      *dest++ = (char16_t)value;
      count++;
    }
    if ((value & 0xE0) == 0xC0) /* U+07FF */
    {
      code = (char16_t)(value & 0x1F) << 6;
      code |= (char16_t)(*src++ & 0x3F);
      *dest++ = code;
      count++;
    }
    if ((value & 0xF0) == 0xE0) /* U+FFFF */
    {
      code = (char16_t)(value & 0x0F) << 12;
      code |= (char16_t)(*src++ & 0x3F) << 6;
      code |= (char16_t)(*src++ & 0x3F);
      *dest++ = code;
      count++;
    }
  }
  *dest = '\0';
  return count;
}

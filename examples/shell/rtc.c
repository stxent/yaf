#include <stdio.h>
#include <time.h>
#include "rtc.h"
//---------------------------------------------------------------------------
//Output string is 9 characters long and contains HH:MM:SS
void timeToStr(char *str, uint16_t value)
{
  sprintf(str, "%02d", ((value >> 11) & 0x1F));
  str[2] = ':';
  sprintf(str + 3, "%02d", ((value >> 5) & 0x3F));
  str[5] = ':';
  sprintf(str + 6, "%02d", (value & 0x1F));
}
//---------------------------------------------------------------------------
//Output string is 11 characters long and contains DD.MM.YYYY
void dateToStr(char *str, uint16_t value)
{
  sprintf(str, "%02d", (value & 0x1F));
  str[2] = '.';
  sprintf(str + 3, "%02d", ((value >> 5) & 0x0F));
  str[5] = '.';
  sprintf(str + 6, "%04d", (((value >> 9) & 0x7F) + 1980));
}
//---------------------------------------------------------------------------
/*15-11 Hours (0-23)
  10-5  Minutes (0-59)
  4-0   Seconds/2 (0-29)*/
//---------------------------------------------------------------------------
uint16_t rtcGetTime()
{
  uint16_t result = 0;
  time_t rawtime;
  struct tm *timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  result |= timeinfo->tm_sec >> 1; //Set seconds
  result |= timeinfo->tm_min << 5; //Set minutes
  result |= timeinfo->tm_hour << 11; //Set hours
  return result;
}
//---------------------------------------------------------------------------
/*15-9  Year (0 = 1980, 127 = 2107)
  8-5   Month (1 = January, 12 = December)
  4-0   Day (1 - 31)*/
//---------------------------------------------------------------------------
uint16_t rtcGetDate()
{
  uint16_t result = 0;
  time_t rawtime;
  struct tm *timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  result |= timeinfo->tm_mday; //Set day
  result |= (timeinfo->tm_mon + 1) << 5; //Set month
  result |= (timeinfo->tm_year - 80) << 9; //Set year
  return result;
}

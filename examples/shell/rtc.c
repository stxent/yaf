#include <stdio.h>
#include <time.h>
#include "rtc.h"
/*----------------------------------------------------------------------------*/
static const int32_t startYear = 1970;
static const uint8_t calendar[] =
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
/*----------------------------------------------------------------------------*/
uint64_t unixTime(struct Time *tm)
{
  /* Stores how many seconds passed from 1.1.1970, 00:00:00 */
  uint64_t s = 0;

  /* If the current year is a leap one than add one day (86400 sec) */
  if (!(tm->year % 4) && (tm->mon > 2))
    s += 86400;

  /* Decrement current month */
  tm->mon--;
  /* Sum the days from January to the current month */
  while (tm->mon)
  {
    /* Add the number of days from a month * 86400 sec */
    s += calendar[--tm->mon] * 86400;
  }
  /* Add:
   * (the number of days from each year (even leap years)) * 86400 sec,
   * the number of days from the current month,
   * the each hour & minute & second from the current day
   */
  s += ((((tm->year - startYear) * 365) + ((tm->year - startYear) >> 2)) *
      (uint64_t)86400) + (tm->day - 1) * (uint32_t)86400 +
      (tm->hour * (uint32_t)3600) + (tm->min * (uint32_t)60) +
      (uint32_t)tm->sec;

  return s;
}
/*----------------------------------------------------------------------------*/
//Output string is 9 characters long and contains HH:MM:SS
void timeToStr(char *str, uint16_t value)
{
  sprintf(str, "%02u", ((value >> 11) & 0x1F));
  str[2] = ':';
  sprintf(str + 3, "%02u", ((value >> 5) & 0x3F));
  str[5] = ':';
  sprintf(str + 6, "%02u", (value & 0x1F));
}
/*----------------------------------------------------------------------------*/
//Output string is 11 characters long and contains DD.MM.YYYY
void dateToStr(char *str, uint16_t value)
{
  sprintf(str, "%02u", (value & 0x1F));
  str[2] = '.';
  sprintf(str + 3, "%02u", ((value >> 5) & 0x0F));
  str[5] = '.';
  sprintf(str + 6, "%04u", (((value >> 9) & 0x7F) + 1980));
}
/*----------------------------------------------------------------------------*/
/*15-11 Hours (0-23)
  10-5  Minutes (0-59)
  4-0   Seconds/2 (0-29)*/
/*----------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------*/
/*15-9  Year (0 = 1980, 127 = 2107)
  8-5   Month (1 = January, 12 = December)
  4-0   Day (1 - 31)*/
/*----------------------------------------------------------------------------*/
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

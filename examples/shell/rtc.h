/*
 * rtc.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef _RTC_H_
#define _RTC_H_
/*----------------------------------------------------------------------------*/
#include <stdint.h>
/*----------------------------------------------------------------------------*/
struct Time {
  uint8_t sec;
  uint8_t min;
  uint8_t hour;
  uint8_t day;
  uint8_t mon;
  int32_t year;
};
/*----------------------------------------------------------------------------*/
// typedef uint64_t UnixTime;
/*----------------------------------------------------------------------------*/
uint64_t unixTime(struct Time *);
uint16_t rtcGetTime();
uint16_t rtcGetDate();
/*----------------------------------------------------------------------------*/
void timeToStr(char *, uint16_t);
void dateToStr(char *, uint16_t);
/*----------------------------------------------------------------------------*/
#endif

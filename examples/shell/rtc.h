/*
 * rtc.h
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef RTC_H_
#define RTC_H_
/*----------------------------------------------------------------------------*/
#include <stdint.h>
#include <error.h>
/*----------------------------------------------------------------------------*/
typedef int64_t time_t;
/*----------------------------------------------------------------------------*/
struct RtcTime
{
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
  uint8_t day;
  uint8_t month;
  int32_t year;
};
/*----------------------------------------------------------------------------*/
time_t rtcEpochTime();
enum result rtcMakeEpochTime(time_t *, const struct RtcTime *);
void rtcMakeTime(struct RtcTime *, time_t);
uint64_t rtcMicrotime();
/*----------------------------------------------------------------------------*/
#endif /* RTC_H_ */

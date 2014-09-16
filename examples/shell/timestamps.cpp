/*
 * timestamps.cpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cstring>
#include "timestamps.hpp"
//------------------------------------------------------------------------------
extern "C"
{
#include <rtc.h>
}
//------------------------------------------------------------------------------
result CurrentDate::run(unsigned int count, const char * const *arguments)
{
  RtcTime currentTime;

  rtcMakeTime(&currentTime, rtcEpochTime());
  owner.log("%02u:%02u:%02u %02u.%02u.%04u", currentTime.hour,
      currentTime.minute, currentTime.second, currentTime.day,
      currentTime.month, currentTime.year);
  return E_OK;
}
//------------------------------------------------------------------------------
result MeasureTime::run(unsigned int count, const char * const *arguments)
{
  uint64_t start, delta;
  result res = E_VALUE;

  for (auto entry : owner.commands())
  {
    if (!strcmp(entry->name(), arguments[0]))
    {
      start = rtcMicrotime();
      res = entry->run(count - 1, arguments + 1);
      delta = rtcMicrotime() - start;

      owner.log("Time passed: %lu us", delta);
    }
  }
  return res;
}

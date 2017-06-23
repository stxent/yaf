/*
 * libshell/timestamps.hpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_LIBSHELL_TIMESTAMPS_HPP_
#define YAF_LIBSHELL_TIMESTAMPS_HPP_

#include <cinttypes>
#include <cstring>
#include "libshell/shell.hpp"

extern "C"
{
#include <xcore/realtime.h>
}

class TimeProvider
{
public:
  virtual ~TimeProvider() = default;
  virtual uint64_t microtime() = 0;
  virtual RtClock *rtc() = 0;
};

template<class T> class CurrentDate: public Shell::ShellCommand
{
public:
  CurrentDate(Shell &parent) : ShellCommand{parent}
  {
  }

  virtual const char *name() const override
  {
    return "date";
  }

  virtual Result run(const char * const *, size_t, Shell::ShellContext *) override
  {
    RtDateTime currentTime;

    rtMakeTime(&currentTime, rtTime(T::instance()->rtc()));
    owner.log("%02u:%02u:%02u %02u.%02u.%04u", currentTime.hour,
        currentTime.minute, currentTime.second, currentTime.day,
        currentTime.month, currentTime.year);
    return E_OK;
  }
};

template<class T> class MeasureTime: public Shell::ShellCommand
{
public:
  MeasureTime(Shell &parent) : ShellCommand{parent}
  {
  }

  virtual const char *name() const override
  {
    return "time";
  }

  virtual Result run(const char * const *arguments, size_t count, Shell::ShellContext *context) override
  {
    uint64_t start, delta;
    Result res = E_VALUE;

    for (auto entry : owner.commands())
    {
      if (!strcmp(entry->name(), arguments[0]))
      {
        start = T::instance()->microtime();
        res = entry->run(arguments + 1, count - 1, context);
        delta = T::instance()->microtime() - start;

        owner.log("Time passed: %"PRIu64" us", delta);
      }
    }
    return res;
  }
};

#endif //YAF_LIBSHELL_TIMESTAMPS_HPP_

/*
 * shell/time_wrapper.cpp
 * Copyright (C) 2015 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#define _POSIX_C_SOURCE 200809L
#define _BSD_SOURCE

#include <cassert>
#include <ctime>
#include "shell/time_wrapper.hpp"

extern "C"
{
#include "shell/unix_time.h"
}
//------------------------------------------------------------------------------
UnixTimeProvider::~UnixTimeProvider()
{
  deinit(clock);
}
//------------------------------------------------------------------------------
uint64_t UnixTimeProvider::microtime()
{
  struct timespec currentTime;

  if (!clock_gettime(CLOCK_REALTIME, &currentTime))
    return currentTime.tv_sec * 1000000 + currentTime.tv_nsec / 1000;
  else
    return 0;
}
//------------------------------------------------------------------------------
RtClock *UnixTimeProvider::rtc()
{
  return clock;
}
//------------------------------------------------------------------------------
UnixTimeProvider *UnixTimeProvider::instance()
{
  static UnixTimeProvider object;

  return &object;
}
//------------------------------------------------------------------------------
UnixTimeProvider::UnixTimeProvider()
{
  clock = reinterpret_cast<RtClock *>(init(UnixTime, nullptr));
  assert(clock != nullptr);
}

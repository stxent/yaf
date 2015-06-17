/*
 * shell/time_wrapper.hpp
 * Copyright (C) 2015 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef SHELL_TIME_WRAPPER_HPP_
#define SHELL_TIME_WRAPPER_HPP_
//------------------------------------------------------------------------------
#include "libshell/timestamps.hpp"

extern "C"
{
#include <realtime.h>
}
//------------------------------------------------------------------------------
class UnixTimeProvider : public TimeProvider
{
public:
  virtual ~UnixTimeProvider();

  virtual uint64_t microtime();
  virtual RtClock *rtc();

  static UnixTimeProvider *instance();

private:
  UnixTimeProvider();
  UnixTimeProvider(const UnixTimeProvider &);
  UnixTimeProvider &operator=(UnixTimeProvider &);

  RtClock *clock;
};
//------------------------------------------------------------------------------
#endif //SHELL_TIME_WRAPPER_HPP_

/*
 * shell/time_wrapper.hpp
 * Copyright (C) 2015 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_SHELL_TIME_WRAPPER_HPP_
#define YAF_SHELL_TIME_WRAPPER_HPP_

#include "libshell/timestamps.hpp"

class UnixTimeProvider: public TimeProvider
{
public:
  UnixTimeProvider(const UnixTimeProvider &) = delete;
  UnixTimeProvider &operator=(UnixTimeProvider &) = delete;

  virtual ~UnixTimeProvider();

  virtual uint64_t microtime();
  virtual RtClock *rtc();

  static UnixTimeProvider *instance();

private:
  UnixTimeProvider();

  RtClock *clock;
};

#endif //YAF_SHELL_TIME_WRAPPER_HPP_

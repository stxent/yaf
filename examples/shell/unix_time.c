/*
 * unix_time.c
 * Copyright (C) 2015 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <time.h>
#include "shell/unix_time.h"
/*----------------------------------------------------------------------------*/
static enum result clkInit(void *, const void *);
static void clkDeinit(void *);
static enum result clkCallback(void *, void (*)(void *), void *);
static enum result clkSetAlarm(void *, time64_t);
static enum result clkSetTime(void *, time64_t);
static time64_t clkTime(void *);
/*----------------------------------------------------------------------------*/
struct UnixTime
{
  struct RtClock parent;
};
/*----------------------------------------------------------------------------*/
static const struct RtClockClass clkTable = {
    .size = sizeof(struct UnixTime),
    .init = clkInit,
    .deinit = clkDeinit,

    .callback = clkCallback,
    .setAlarm = clkSetAlarm,
    .setTime = clkSetTime,
    .time = clkTime
};
/*----------------------------------------------------------------------------*/
const struct RtClockClass * const UnixTime = &clkTable;
/*----------------------------------------------------------------------------*/
static enum result clkInit(void *object __attribute__((unused)),
    const void *configBase __attribute__((unused)))
{
  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void clkDeinit(void *object __attribute__((unused)))
{

}
/*----------------------------------------------------------------------------*/
static enum result clkCallback(void *object __attribute__((unused)),
    void (*callback)(void *) __attribute__((unused)),
    void *argument __attribute__((unused)))
{
  return E_ERROR;
}
/*----------------------------------------------------------------------------*/
static enum result clkSetAlarm(void *object __attribute__((unused)),
    time64_t alarmTime __attribute__((unused)))
{
  return E_ERROR;
}
/*----------------------------------------------------------------------------*/
static enum result clkSetTime(void *object __attribute__((unused)),
    time64_t currentTime __attribute__((unused)))
{
  return E_ERROR;
}
/*----------------------------------------------------------------------------*/
static time64_t clkTime(void *object __attribute__((unused)))
{
  return (time64_t)time(0);
}

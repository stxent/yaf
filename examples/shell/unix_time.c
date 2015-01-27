/*
 * unix_time.c
 * Copyright (C) 2015 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <time.h>
#include "shell/unix_time.h"
/*----------------------------------------------------------------------------*/
static enum result tmrInit(void *, const void *);
static void tmrDeinit(void *);
static enum result tmrCallback(void *, void (*)(void *), void *);
static enum result tmrSetAlarm(void *, time64_t);
static enum result tmrSetTime(void *, time64_t);
static time64_t tmrTime(void *);
/*----------------------------------------------------------------------------*/
struct UnixTime
{
  struct Rtc parent;
};
/*----------------------------------------------------------------------------*/
static const struct RtcClass tmrTable = {
    .size = sizeof(struct UnixTime),
    .init = tmrInit,
    .deinit = tmrDeinit,

    .callback = tmrCallback,
    .setAlarm = tmrSetAlarm,
    .setTime = tmrSetTime,
    .time = tmrTime
};
/*----------------------------------------------------------------------------*/
const struct RtcClass * const UnixTime = &tmrTable;
/*----------------------------------------------------------------------------*/
static enum result tmrInit(void *object __attribute__((unused)),
    const void *configBase __attribute__((unused)))
{
  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void tmrDeinit(void *object __attribute__((unused)))
{

}
/*----------------------------------------------------------------------------*/
static enum result tmrCallback(void *object __attribute__((unused)),
    void (*callback)(void *) __attribute__((unused)),
    void *callbackArgument __attribute__((unused)))
{
  return E_ERROR;
}
/*----------------------------------------------------------------------------*/
static enum result tmrSetAlarm(void *object __attribute__((unused)),
    time64_t alarmTime __attribute__((unused)))
{
  return E_ERROR;
}
/*----------------------------------------------------------------------------*/
static enum result tmrSetTime(void *object __attribute__((unused)),
    time64_t currentTime __attribute__((unused)))
{
  return E_ERROR;
}
/*----------------------------------------------------------------------------*/
static time64_t tmrTime(void *object __attribute__((unused)))
{
  return (time64_t)time(0);
}

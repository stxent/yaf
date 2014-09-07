/*
 * thread.h
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef THREAD_H_
#define THREAD_H_
/*----------------------------------------------------------------------------*/
#include <stdbool.h>
#include <error.h>
/*----------------------------------------------------------------------------*/
struct Thread
{
  void (*onTerminateCallback)(void *);
  void *onTerminateArgument;

  void *handle;
};
/*----------------------------------------------------------------------------*/
enum result threadInit(struct Thread *, unsigned int, int, void (*)(void *),
    void *);
void threadDeinit(struct Thread *);
/*----------------------------------------------------------------------------*/
void threadOnTerminateCallback(struct Thread *, void (*)(void *), void *);
enum result threadStart(struct Thread *);
void threadTerminate(struct Thread *);
/*----------------------------------------------------------------------------*/
void msleep(unsigned int);
/*----------------------------------------------------------------------------*/
#endif /* THREAD_H_ */

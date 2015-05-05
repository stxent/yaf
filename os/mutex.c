/*
 * mutex.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#define _POSIX_C_SOURCE 200809L
#define _BSD_SOURCE

#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <os/mutex.h>
/*----------------------------------------------------------------------------*/
enum result mutexInit(struct Mutex *mutex)
{
  mutex->handle = malloc(sizeof(pthread_mutex_t));
  if (!mutex->handle)
    return E_MEMORY;
  if (pthread_mutex_init(mutex->handle, 0))
    return E_ERROR;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
void mutexDeinit(struct Mutex *mutex)
{
  pthread_mutex_destroy(mutex->handle);
  free(mutex->handle);
}
/*----------------------------------------------------------------------------*/
void mutexLock(struct Mutex *mutex)
{
  pthread_mutex_lock(mutex->handle);
}
/*----------------------------------------------------------------------------*/
bool mutexTryLock(struct Mutex *mutex, unsigned int interval)
{
  int res;

  if (interval)
  {
    struct timespec timestamp;

    clock_gettime(CLOCK_REALTIME, &timestamp);
    timestamp.tv_sec += interval / 1000;
    timestamp.tv_nsec += (interval % 1000) * 1000000;

    if (timestamp.tv_nsec >= 1000000000)
    {
      timestamp.tv_nsec -= 1000000000;
      ++timestamp.tv_sec;
    }

    res = pthread_mutex_timedlock(mutex->handle, &timestamp);
  }
  else
    res = pthread_mutex_trylock(mutex->handle);

  return res ? false : true;
}
/*----------------------------------------------------------------------------*/
void mutexUnlock(struct Mutex *mutex)
{
  pthread_mutex_unlock(mutex->handle);
}

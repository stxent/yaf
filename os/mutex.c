/*
 * mutex.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <pthread.h>
#include <stdlib.h>
#include <mutex.h>
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
bool mutexTryLock(struct Mutex *mutex)
{
  return pthread_mutex_trylock(mutex->handle) ? false : true;
}
/*----------------------------------------------------------------------------*/
void mutexUnlock(struct Mutex *mutex)
{
  pthread_mutex_unlock(mutex->handle);
}

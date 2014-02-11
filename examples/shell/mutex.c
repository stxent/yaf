/*
 * mutex.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <pthread.h>
#include <stdlib.h>
#include <mutex.h>
/*----------------------------------------------------------------------------*/
enum result mutexInit(struct Mutex *m)
{
  m->handle = malloc(sizeof(pthread_mutex_t));
  if (!m->handle)
    return E_MEMORY;
  if (pthread_mutex_init(m->handle, 0))
    return E_ERROR;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
void mutexDeinit(struct Mutex *m)
{
  pthread_mutex_destroy(m->handle);
}
/*----------------------------------------------------------------------------*/
void mutexLock(struct Mutex *m)
{
  pthread_mutex_lock(m->handle);
}
/*----------------------------------------------------------------------------*/
bool mutexTryLock(struct Mutex *m)
{
  return pthread_mutex_trylock(m->handle) ? false : true;
}
/*----------------------------------------------------------------------------*/
void mutexUnlock(struct Mutex *m)
{
  pthread_mutex_unlock(m->handle);
}
